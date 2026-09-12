#include "conn_table.h"

#include <stdlib.h>

#include "connection_internal.h"

struct conn_table {
    const server_config *cfg;
    connection          *conns;
    struct pollfd       *fds;
    size_t               count;
    size_t               cap;
};

conn_table *conn_table_new(const server_config *cfg) {
    conn_table *t = calloc(1, sizeof(*t));
    if (t == NULL) {
        return NULL;
    }

    //initialize connection array and pollfd array
    // this two arrays are used to multiplex connections
    t->conns = calloc(cfg->max_connections, sizeof(*t->conns));
    t->fds   = calloc(cfg->max_connections + 1, sizeof(*t->fds));

    if (t->conns == NULL || t->fds == NULL) {
        free(t->conns);
        free(t->fds);
        free(t);
        return NULL;
    }

    t->cfg   = cfg;
    t->cap   = cfg->max_connections;
    t->count = 0;
    return t;
}

static void drop(conn_table *t, size_t index) {
    connection_close(&t->conns[index]);
    //swap-remove for drop function
    t->conns[index] = t->conns[t->count - 1];
    t->count -= 1;
}

void conn_table_free(conn_table *t) {
    if (t == NULL) {
        return;
    }

    while (t->count > 0) {
        drop(t, t->count - 1);
    }

    free(t->conns);
    free(t->fds);
    free(t);
}

int conn_table_full(const conn_table *t) {
    return t->count >= t->cap;
}

size_t conn_table_count(const conn_table *t) {
    return t->count;
}

int conn_table_admit(conn_table *t, int fd, time_t now) {
    if (conn_table_full(t)) {
        return -1;
    }

    connection_open(&t->conns[t->count], fd, t->cfg, now);
    t->count += 1;
    return 0;
}

struct pollfd *conn_table_arm(conn_table *t, int listen_fd, nfds_t *nfds) {
    t->fds[0].fd      = conn_table_full(t) ? -1 : listen_fd;
    t->fds[0].events  = POLLIN;
    t->fds[0].revents = 0;

    size_t i = 0;
    //skiped this loops while count is 0
    while (i < t->count) {
        t->fds[i + 1].fd      = connection_fd(&t->conns[i]);
        t->fds[i + 1].events  = connection_interest(&t->conns[i]);
        t->fds[i + 1].revents = 0;
        i++;
    }

    *nfds = (nfds_t)(t->count + 1);
    return t->fds;
}

short conn_table_listener_revents(const conn_table *t) {
    return t->fds[0].revents;
}

void conn_table_dispatch(conn_table *t, time_t now) {
    size_t i = t->count;

    while (i > 0) {
        i--;

        short revents = t->fds[i + 1].revents;
        if (revents == 0) {
            continue;
        }

        if (connection_on_ready(&t->conns[i], revents, now) == -1) {
            drop(t, i);
        }
    }
}

void conn_table_expire(conn_table *t, time_t now) {
    size_t i = t->count;

    while (i > 0) {
        i--;

        if (connection_on_clock(&t->conns[i], now) == -1) {
            drop(t, i);
        }
    }
}
