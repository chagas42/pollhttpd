/* Builds an HTTP response into a buffer ready to be written. */
#pragma once

#include <stddef.h>

typedef struct {
    char  *data;        /* headers + body */
    size_t len;
    size_t headers_len; /* headers only: HEAD sends up to here */
} http_response;

/* Builds the response. Returns 0, or -1 on allocation failure.
 * The caller takes ownership of `res->data`. */
int http_response_build(http_response *res, int status, const char *reason,
                        const char *media_type, const char *body,
                        size_t body_len, int keep_alive);

void http_response_free(http_response *res);

/* Default reason phrase for the status. Never returns NULL. */
const char *http_status_reason(int status);
