#pragma once

#include <stddef.h>

typedef struct {
    const char *port;              // "0" asks the kernel for an ephemeral one
    const char *root;
    size_t      max_connections;
    int         idle_timeout_s;    // between requests
    int         request_timeout_s; // absolute, from the first byte of a request
    int         poll_timeout_ms;
} server_config;

server_config server_config_defaults(void);
