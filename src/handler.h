#pragma once

#include <stdbool.h>

#include "http_parser.h"
#include "http_response.h"

typedef struct {
    int    ok;        // 0 built, -1 failed
    size_t to_send;
} handler_result;

handler_result handler_reply(const http_request *req, http_parse_result parsed,
                             bool keep_alive, const char *root,
                             http_response *res);

int handler_error(http_response *res, int status);
