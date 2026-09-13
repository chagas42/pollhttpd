#include "conn_table.h"

#include <stdlib.h>
#include <stdbool.h>

// for the complete type only: sizeof and indexing, never a named symbol
#include "connection_internal.h"  // IWYU pragma: keep

struct conn_table {
    const server_config *cfg;
    connection *conns;
    struct pollfd *fds;
    size_t count;
    size_t cap;
};

conn_table *conn_table_new(const server_config *cfg) {
    conn_table *table_pointer = calloc(1, sizeof(*table_pointer));
    if (table_pointer == NULL) {
        return NULL;
    }

    //initialize connection array and pollfd array
    // this two arrays are used to multiplex connections
    table_pointer->conns = calloc(cfg->max_connections, sizeof(*table_pointer->conns));
    table_pointer->fds   = calloc(cfg->max_connections + 1, sizeof(*table_pointer->fds));

    if (table_pointer->conns == NULL || table_pointer->fds == NULL) {
      //cleanup on error to provide memory safety
        free(table_pointer->conns);
        free(table_pointer->fds);
        free(table_pointer);
        return NULL;
    }

    table_pointer->cfg = cfg;
    table_pointer->cap = cfg->max_connections;
    table_pointer->count = 0;
    return table_pointer;
}

// true enquanto o indice ainda aponta para uma conexao viva. como funcao,
// a condicao e reavaliada a cada volta -- um bool guardado antes do laco
// congela no valor de entrada
static bool has_work(const conn_table *table_pointer, size_t index) {
    return index < table_pointer->count;
}

static void drop(conn_table *table_pointer, size_t index) {
    connection_close(&table_pointer->conns[index]);
    //swap-remove for drop function
    table_pointer->conns[index] = table_pointer->conns[table_pointer->count - 1];
    table_pointer->count -= 1;
}

void conn_table_free(conn_table *table_pointer) {
    if (table_pointer == NULL) {
        return;
    }

    while (table_pointer->count > 0) {
        drop(table_pointer, table_pointer->count - 1);
    }

    free(table_pointer->conns);
    free(table_pointer->fds);
    free(table_pointer);
}

bool conn_table_is_full(const conn_table *table_pointer) {
    return table_pointer->count >= table_pointer->cap;
}

size_t conn_table_count(const conn_table *table_pointer) {
    return table_pointer->count;
}

int conn_table_admit(conn_table *table_pointer, int fd, time_t now) {
    if (conn_table_is_full(table_pointer)) {
        return -1;
    }

    connection_open(&table_pointer->conns[table_pointer->count], fd, table_pointer->cfg, now);
    table_pointer->count += 1;
    return 0;
}

poll_set conn_table_prepare_poll(conn_table *table_pointer, int listen_fd) {
    table_pointer->fds[0].fd = conn_table_is_full(table_pointer) ? -1 : listen_fd;
    table_pointer->fds[0].events = POLLIN;
    table_pointer->fds[0].revents = 0;

    size_t i = 0;

    //skiped this loops while count is 0
    while (has_work(table_pointer, i)) {
        table_pointer->fds[i + 1].fd = connection_fd(&table_pointer->conns[i]);
        table_pointer->fds[i + 1].events = connection_interest(&table_pointer->conns[i]);
        table_pointer->fds[i + 1].revents = 0;
        i++;
    }

    poll_set set = {
        .fds = table_pointer->fds,
        .count = (nfds_t)(table_pointer->count + 1),
    };
    return set;
}

short conn_table_listener_revents(const conn_table *table_pointer) {
    return table_pointer->fds[0].revents;
}

void conn_table_dispatch(conn_table *table_pointer, time_t now) {
    size_t i = table_pointer->count;

    while (i > 0) {
        i--;

        short revents = table_pointer->fds[i + 1].revents;
        if (revents == 0) {
            continue;
        }

        if (connection_on_ready(&table_pointer->conns[i], revents, now) == -1) {
            drop(table_pointer, i);
        }
    }
}

void conn_table_expire(conn_table *table_pointer, time_t now) {
    size_t i = table_pointer->count;

    while (i > 0) {
        i--;

        if (connection_on_clock(&table_pointer->conns[i], now) == -1) {
            drop(table_pointer, i);
        }
    }
}
