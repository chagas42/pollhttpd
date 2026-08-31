/* Per-connection state: parser, output buffer and lifecycle.
 *
 * Each connection embeds its own parser — no allocation per connection,
 * and two connections never share state. */
#pragma once

#include <time.h>

#include "http_parser.h"
#include "http_response.h"

typedef enum {
    CONN_READING,
    CONN_WRITING,
} conn_state;

typedef struct {
    int           fd;
    conn_state    state;
    http_parser   parser;
    http_response out;
    size_t        sent;
    size_t        to_send;
    int           keep_alive;
    time_t        last_activity;
} connection;

void connection_open(connection *conn, int fd);
void connection_close(connection *conn);

/* Return 0 to keep the connection alive, -1 to close it. */
int connection_on_readable(connection *conn);
int connection_on_writable(connection *conn);
