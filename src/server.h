#pragma once

#include <time.h>

#include "config.h"

typedef struct server server;

int  server_listen(const server_config *cfg, server **out);
int  server_port(const server *s);

// one full turn: arm, poll, dispatch, accept, expire. -1 is fatal.
int  server_tick(server *s, time_t now);

void server_stop(server *s);

int  server_run(const server_config *cfg);
