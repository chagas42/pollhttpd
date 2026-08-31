#include "server.h"

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <poll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "connection.h"
#include "handler.h"
#include "log.h"

#define LOG_LABEL "server"
#define LISTEN_BACKLOG 128
#define MAX_CONNECTIONS  64
#define POLL_TIMEOUT_MS 1000

#define IDLE_TIMEOUT_S     15
#define REQUEST_TIMEOUT_S  10

static int bind_to_address(const struct addrinfo *addr) {
    int fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (fd == -1) {
        log_errno("socket");
        return -1;
    }

    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        log_errno("setsockopt(SO_REUSEADDR)");
        close(fd);
        return -1;
    }

    if (addr->ai_family == AF_INET6) {
        int v6only = 0;
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
                       &v6only, sizeof(v6only)) == -1) {
            log_errno("setsockopt(IPV6_V6ONLY)");
            close(fd);
            return -1;
        }
    }

    if (bind(fd, addr->ai_addr, addr->ai_addrlen) == -1) {
        log_errno("bind");
        close(fd);
        return -1;
    }

    return fd;
}

static int bind_first_of_family(struct addrinfo *candidates, int family) {
    struct addrinfo *addr = candidates;
    int fd = -1;

    while (addr != NULL && fd == -1) {
        if (addr->ai_family == family) {
            fd = bind_to_address(addr);
        }
        addr = addr->ai_next;
    }

    return fd;
}

static int listen_socket_open(const char *port) {
    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags    = AI_PASSIVE,
    };

    struct addrinfo *candidates = NULL;
    int err = getaddrinfo(NULL, port, &hints, &candidates);
    if (err != 0) {
        log_error("getaddrinfo: %s", gai_strerror(err));
        return -1;
    }

    int listen_fd = bind_first_of_family(candidates, AF_INET6);
    if (listen_fd == -1) {
        listen_fd = bind_first_of_family(candidates, AF_INET);
    }

    freeaddrinfo(candidates);

    if (listen_fd == -1) {
        log_error("no usable address for port %s", port);
        return -1;
    }

    if (listen(listen_fd, LISTEN_BACKLOG) == -1) {
        log_errno("listen");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        return -1;
    }

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void accept_new(int listen_fd, connection *conns, size_t *count) {
    int client_fd = accept(listen_fd, NULL, NULL);

    if (client_fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            log_errno("accept");
        }
        return;
    }

    if (*count >= MAX_CONNECTIONS) {
        log_error("connection limit of %d reached", MAX_CONNECTIONS);
        close(client_fd);
        return;
    }

    if (set_nonblocking(client_fd) == -1) {
        log_errno("fcntl(O_NONBLOCK)");
        close(client_fd);
        return;
    }

    connection_open(&conns[*count], client_fd);
    *count += 1;
}

static void drop(connection *conns, size_t *count, size_t index) {
    connection_close(&conns[index]);
    conns[index] = conns[*count - 1];
    *count -= 1;
}

static void expire_idle(connection *conns, size_t *count) {
    time_t now = time(NULL);
    size_t i = *count;

    while (i > 0) {
        i--;

        int    started = http_parser_started(&conns[i].parser);
        double limit = started ? REQUEST_TIMEOUT_S : IDLE_TIMEOUT_S;

        if (difftime(now, conns[i].last_activity) <= limit) {
            continue;
        }

        if (started && conns[i].state == CONN_READING &&
            handler_error(&conns[i].out, 408) == 0) {
            log_info("408 for a connection stalled mid-request");
            conns[i].keep_alive = 0;
            conns[i].to_send = conns[i].out.len;
            conns[i].sent = 0;
            conns[i].state = CONN_WRITING;
            conns[i].last_activity = now;
            continue;
        }

        drop(conns, count, i);
    }
}

int server_run(const char *port) {
    int listen_fd = listen_socket_open(port);
    if (listen_fd == -1) {
        return -1;
    }

    if (set_nonblocking(listen_fd) == -1) {
        log_errno("fcntl(O_NONBLOCK)");
        close(listen_fd);
        return -1;
    }

    connection    conns[MAX_CONNECTIONS];
    struct pollfd fds[MAX_CONNECTIONS + 1];
    size_t        count = 0;

    log_info("listening on http://localhost:%s", port);

    while (1) {
        fds[0].fd = listen_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        size_t i = 0;
        while (i < count) {
            fds[i + 1].fd = conns[i].fd;
            fds[i + 1].events =
                (conns[i].state == CONN_READING) ? POLLIN : POLLOUT;
            fds[i + 1].revents = 0;
            i++;
        }

        if (poll(fds, (nfds_t)(count + 1), POLL_TIMEOUT_MS) == -1) {
            if (errno == EINTR) {
                continue;
            }
            log_errno("poll");
            break;
        }

        i = count;
        while (i > 0) {
            i--;

            short revents = fds[i + 1].revents;
            if (revents == 0) {
                continue;
            }

            int keep;
            if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
                keep = -1;
            } else if (conns[i].state == CONN_READING) {
                keep = connection_on_readable(&conns[i]);
            } else {
                keep = connection_on_writable(&conns[i]);
            }

            if (keep == -1) {
                drop(conns, &count, i);
            }
        }

        if (fds[0].revents & POLLIN) {
            accept_new(listen_fd, conns, &count);
        }

        expire_idle(conns, &count);
    }

    close(listen_fd);
    return -1;
}
