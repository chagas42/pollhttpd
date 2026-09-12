#include "connection_internal.h"

#include <poll.h>
#include <unistd.h>

#include "handler.h"
#include "log.h"

#define LOG_LABEL "connection"

// the one place that moves a connection from reading to writing
static void begin_response(
  connection *conn,
  size_t to_send,
  int keep_alive,
  time_t now
) {
    conn->to_send = to_send;
    conn->sent = 0;
    conn->keep_alive = keep_alive;
    conn->state = CONN_WRITING;
    conn->deadline = now + conn->cfg->request_timeout_s;
}

void connection_open(
  connection *conn,
  int fd,
  const server_config *cfg,
  time_t now
) {
    conn->fd = fd;
    conn->cfg = cfg;
    conn->state = CONN_READING;
    http_parser_init(&conn->parser);
    conn->out.data = NULL;
    conn->out.len = 0;
    conn->out.headers_len = 0;
    conn->sent = 0;
    conn->to_send = 0;
    conn->keep_alive = 0;
    conn->idle_since = now;
    conn->deadline = 0;
    conn->in_len = 0;
    conn->in_pos = 0;
}

void connection_close(connection *conn) {
    http_response_free(&conn->out);
    close(conn->fd);
    conn->fd = -1;
}

int connection_fd(const connection *conn) {
    return conn->fd;
}

short connection_interest(const connection *conn) {
    return (conn->state == CONN_READING) ? POLLIN : POLLOUT;
}

static void connection_reset(connection *conn, time_t now) {
    http_response_free(&conn->out);
    http_parser_init(&conn->parser);
    conn->sent = 0;
    conn->to_send = 0;
    conn->state = CONN_READING;
    conn->idle_since = now;
    conn->deadline = 0;
}

static int reply(connection *conn, http_parse_result parsed, time_t now) {
    int keep_alive =
        (parsed == HTTP_PARSE_OK)
            ? http_request_wants_keep_alive(http_parser_request(&conn->parser))
            : 0;

    handler_result r = handler_reply(http_parser_request(&conn->parser),
                                     parsed, keep_alive,
                                     conn->cfg->root, &conn->out);
    if (r.ok == -1) {
        log_error("could not build the response");
        return -1;
    }

    begin_response(conn, r.to_send, keep_alive, now);
    return 0;
}

// stops at the first request that turns into a response: only one message
// is in flight at a time, so the rest stays buffered
static int feed_buffered(connection *conn, time_t now) {
    if (conn->in_pos >= conn->in_len) {
        conn->in_pos = 0;
        conn->in_len = 0;
        return 0;
    }

    // the first byte of a request arms the absolute deadline; later ones do not
    if (conn->deadline == 0) {
        conn->deadline = now + conn->cfg->request_timeout_s;
    }

    http_feed_result r = http_parser_feed(&conn->parser,
                                          conn->in + conn->in_pos,
                                          conn->in_len - conn->in_pos);
    conn->in_pos += r.consumed;

    if (r.status == HTTP_PARSE_INCOMPLETE) {
        // the parser holds the partial request, so the buffer can be refilled
        conn->in_pos = 0;
        conn->in_len = 0;
        return 0;
    }

    return reply(conn, r.status, now);
}

static int on_readable(connection *conn, time_t now) {
    for (;;) {
        if (feed_buffered(conn, now) == -1) {
            return -1;
        }

        if (conn->state != CONN_READING) {
            return 0;
        }

        ssize_t n = read(conn->fd, conn->in, sizeof(conn->in));

        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            if (errno == EINTR) {
                continue;
            }
            log_errno("read");
            return -1;
        }

        if (n == 0) {
            return -1;
        }

        conn->in_len = (size_t)n;
        conn->in_pos = 0;
    }
}

static int on_writable(connection *conn, time_t now) {
    while (conn->sent < conn->to_send) {
        ssize_t n = write(conn->fd, conn->out.data + conn->sent,
                          conn->to_send - conn->sent);

        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            if (errno == EINTR) {
                continue;
            }
            log_errno("write");
            return -1;
        }

        conn->sent += (size_t)n;
    }

    if (!conn->keep_alive) {
        return -1;
    }

    connection_reset(conn, now);

    // a pipelined request may already be sitting in the buffer
    return feed_buffered(conn, now);
}

int connection_on_ready(connection *conn, short revents, time_t now) {
    //bitwise OR to check for fallback statuses
    if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return -1;
    }

    if (conn->state == CONN_READING) {
        return on_readable(conn, now);
    }

    return on_writable(conn, now);
}

int connection_on_clock(connection *conn, time_t now) {
    if (conn->deadline != 0) {
        if (now < conn->deadline) {
            return 0;
        }

        if (conn->state == CONN_READING && handler_error(&conn->out, 408) == 0) {
            log_info("408 for a connection stalled mid-request");
            begin_response(conn, conn->out.len, 0, now);
            return 0;
        }

        return -1;
    }

    if (difftime(now, conn->idle_since) > conn->cfg->idle_timeout_s) {
        return -1;
    }

    return 0;
}
