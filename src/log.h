/* Logging with a per-module label.
 *
 * Each .c defines LOG_LABEL with its module name before including this
 * header, so output can be filtered by origin.
 *
 * Everything goes to stderr: stdout may be redirected as data. */
#pragma once

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define log_info(fmt, ...) \
    fprintf(stderr, "[%s] " fmt "\n", LOG_LABEL, ##__VA_ARGS__)

#define log_error(fmt, ...) \
    fprintf(stderr, "[%s] erro: " fmt "\n", LOG_LABEL, ##__VA_ARGS__)

/* Syscall failure: appends the errno cause automatically, so callers
 * need not remember to print strerror themselves. */
#define log_errno(call) \
    fprintf(stderr, "[%s] erro: %s: %s\n", LOG_LABEL, (call), strerror(errno))
