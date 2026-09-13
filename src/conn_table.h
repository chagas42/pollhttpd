#pragma once

#include <poll.h>
#include <stdbool.h>
#include <time.h>

#include "config.h"
#include "connection.h"

// owns the +1 offset: connection i lives at fds[i + 1], slot 0 is the
// listener. removal is swap-remove, so every walk runs backwards.
typedef struct conn_table conn_table;

conn_table *conn_table_new(const server_config *cfg);
void conn_table_free(conn_table *t);

bool conn_table_is_full(const conn_table *table_pointer);
size_t conn_table_count(const conn_table *t);

// takes an accepted fd. returns 0, or -1 if the table is full.
int conn_table_admit(conn_table *t, int fd, time_t now);

// the two arguments poll() needs, so they cannot be passed from different
// rounds by mistake
typedef struct {
    struct pollfd *fds;
    nfds_t         count;
} poll_set;

// rebuilds the array from the current table. when full, the listener slot
// gets fd -1, which poll() ignores: clients then wait in the backlog
// instead of being accepted and closed
poll_set conn_table_prepare_poll(conn_table *table_pointer, int listen_fd);

short conn_table_listener_revents(const conn_table *t);

void conn_table_dispatch(conn_table *t, time_t now);
void conn_table_expire(conn_table *t, time_t now);
