#pragma once

#include <stddef.h>

typedef struct {
    char  *data;
    size_t len;
    size_t headers_len;
} http_response;

// reason is derived from status, not passed: the two cannot disagree
int http_response_build(http_response *res, int status,
                        const char *media_type, const char *body,
                        size_t body_len, int keep_alive);

void http_response_free(http_response *res);

const char *http_status_reason(int status);
