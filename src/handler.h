#pragma once

#include "http_parser.h"
#include "http_response.h"

int handler_reply(const http_parser *parser, http_parse_result parsed,
                  int keep_alive, http_response *res, int *head_only);

int handler_error(http_response *res, int status);
