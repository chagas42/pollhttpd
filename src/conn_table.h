#pragma once

#include <poll.h>
#include <time.h>

#include "config.h"
#include "connection.h"

// owns the +1 offset: connection i lives at fds[i + 1], slot 0 is the
// listener. removal is swap-remove, so every walk runs backwards.
typedef struct conn_table conn_table;

conn_table *conn_table_new(const server_config *cfg);
void        conn_table_free(conn_table *t);

int    conn_table_full(const conn_table *t);
size_t conn_table_count(const conn_table *t);

// takes an accepted fd. returns 0, or -1 if the table is full.
int    conn_table_admit(conn_table *t, int fd, time_t now);

// when full, the listener slot gets fd -1, which poll() ignores: clients
// then wait in the backlog instead of being accepted and closed
struct pollfd *conn_table_arm(conn_table *t, int listen_fd, nfds_t *nfds);

short  conn_table_listener_revents(const conn_table *t);

void   conn_table_dispatch(conn_table *t, time_t now);
void   conn_table_expire(conn_table *t, time_t now);
