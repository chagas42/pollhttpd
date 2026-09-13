#include "server.h"

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stddef.h>
#include <poll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "conn_table.h"
#include "log.h"

#define LOG_LABEL "server"
#define LISTEN_BACKLOG 128

struct server {
    server_config cfg;
    int listen_fd;
    conn_table *table;
};

server_config server_config_defaults(void) {
    server_config cfg = {
        .port = "8080",
        .root = "www",
        .max_connections = 64,
        .idle_timeout_s = 15,
        .request_timeout_s = 10,
        .poll_timeout_ms = 1000,
    };
    return cfg;
}

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

static void accept_new(server *s, time_t now) {
    int client_fd = accept(s->listen_fd, NULL, NULL);

    if (client_fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            log_errno("accept");
        }
        return;
    }

    if (set_nonblocking(client_fd) == -1) {
        log_errno("fcntl(O_NONBLOCK)");
        close(client_fd);
        return;
    }

    if (conn_table_admit(s->table, client_fd, now) == -1) {
        log_error("connection limit of %zu reached", s->cfg.max_connections);
        close(client_fd);
    }
}

int server_listen(const server_config *cfg, server **out) {
    //error validation
    *out = NULL;

    int listen_fd = listen_socket_open(cfg->port);
    if (listen_fd == -1) {
        return -1;
    }

    if (set_nonblocking(listen_fd) == -1) {
        log_errno("fcntl(O_NONBLOCK)");
        close(listen_fd);
        return -1;
    }

    server *s = calloc(1, sizeof(*s));
    if (s == NULL) {
        close(listen_fd);
        return -1;
    }

    s->cfg = *cfg;
    s->listen_fd = listen_fd;
    s->table = conn_table_new(&s->cfg);

    if (s->table == NULL) {
        free(s);
        close(listen_fd);
        return -1;
    }

    *out = s;
    return 0;
}

int server_port(const server *s) {
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);

    if (getsockname(s->listen_fd, (struct sockaddr *)&addr, &len) == -1) {
        log_errno("getsockname");
        return -1;
    }

    if (addr.ss_family == AF_INET6) {
        return ntohs(((struct sockaddr_in6 *)&addr)->sin6_port);
    }

    return ntohs(((struct sockaddr_in *)&addr)->sin_port);
}

int server_tick(server *s, time_t now) {
    poll_set set = conn_table_prepare_poll(s->table, s->listen_fd);

    if (poll(set.fds, set.count, s->cfg.poll_timeout_ms) == -1) {
        if (errno == EINTR) {
            return 0;
        }
        log_errno("poll");
        return -1;
    }

    conn_table_dispatch(s->table, now);

    // after dispatch: a slot admitted earlier would carry this round's revents
    if (conn_table_listener_revents(s->table) & POLLIN) {
        accept_new(s, now);
    }

    conn_table_expire(s->table, now);
    return 0;
}

void server_stop(server *s) {
    if (s == NULL) {
        return;
    }

    conn_table_free(s->table);
    close(s->listen_fd);
    free(s);
}

int server_run(const server_config *cfg) {
    server *server_pointer = NULL;

    if (server_listen(cfg, &server_pointer) == -1) {
        return -1;
    }

    log_info("listening on http://localhost:%d", server_port(server_pointer));

    while (server_tick(server_pointer, time(NULL)) == 0) {
        ;
    }

    server_stop(server_pointer);
    return -1;
}
