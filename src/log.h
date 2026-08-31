#pragma once

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define log_info(fmt, ...) \
    fprintf(stderr, "[%s] " fmt "\n", LOG_LABEL, ##__VA_ARGS__)

#define log_error(fmt, ...) \
    fprintf(stderr, "[%s] error: " fmt "\n", LOG_LABEL, ##__VA_ARGS__)

#define log_errno(call) \
    fprintf(stderr, "[%s] erro: %s: %s\n", LOG_LABEL, (call), strerror(errno))
