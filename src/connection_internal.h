#pragma once

// private to connection.c and conn_table.c: nobody else sees the fields

#include <stdbool.h>

#include "connection.h"
#include "http_parser.h"
#include "http_response.h"

#define CONN_READ_CHUNK 4096

typedef enum {
    CONN_READING,
    CONN_WRITING,
} conn_state;

struct connection {
    int fd;
    const server_config *cfg;
    conn_state state;
    http_parser parser;
    http_response out;
    size_t sent;
    size_t to_send;
    bool keep_alive;
    time_t idle_since;
    time_t deadline;   // 0 when nothing is in flight

    // in_pos is the parser's cursor; past it is the next pipelined request
    char in[CONN_READ_CHUNK];
    size_t in_len;
    size_t in_pos;
};
