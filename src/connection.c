#include "connection.h"

#include <unistd.h>

#include "handler.h"
#include "log.h"

#define LOG_LABEL "connection"
#define READ_CHUNK 4096

void connection_open(connection *conn, int fd) {
    conn->fd = fd;
    conn->state = CONN_READING;
    http_parser_init(&conn->parser);
    conn->out.data = NULL;
    conn->out.len = 0;
    conn->out.headers_len = 0;
    conn->sent = 0;
    conn->to_send = 0;
    conn->keep_alive = 0;
    conn->last_activity = time(NULL);
}

void connection_close(connection *conn) {
    http_response_free(&conn->out);
    close(conn->fd);
    conn->fd = -1;
}

static void connection_reset(connection *conn) {
    http_response_free(&conn->out);
    http_parser_init(&conn->parser);
    conn->sent = 0;
    conn->to_send = 0;
    conn->state = CONN_READING;
}

int connection_on_readable(connection *conn) {
    char buf[READ_CHUNK];
    ssize_t n = read(conn->fd, buf, sizeof(buf));

    if (n == -1) {

        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        log_errno("read");
        return -1;
    }

    if (n == 0) {
        return -1;
    }

    conn->last_activity = time(NULL);

    http_parse_result parsed =
        http_parser_feed(&conn->parser, buf, (size_t)n);

    if (parsed == HTTP_PARSE_INCOMPLETE) {
        return 0;
    }

    conn->keep_alive =
        (parsed == HTTP_PARSE_OK)
            ? http_request_wants_keep_alive(http_parser_request(&conn->parser))
            : 0;

    int head_only = 0;
    if (handler_reply(&conn->parser, parsed, conn->keep_alive,
                      &conn->out, &head_only) == -1) {
        log_error("could not build the response");
        return -1;
    }

    conn->to_send = head_only ? conn->out.headers_len : conn->out.len;
    conn->sent = 0;
    conn->state = CONN_WRITING;
    return 0;
}

int connection_on_writable(connection *conn) {
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

    conn->last_activity = time(NULL);

    if (!conn->keep_alive) {
        return -1;
    }

    connection_reset(conn);
    return 0;
}
