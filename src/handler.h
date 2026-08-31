/* Turns a parse result into a ready-to-send response. */
#pragma once

#include "http_parser.h"
#include "http_response.h"

/* Builds the response for `parsed`. Returns 0, or -1 on allocation
 * failure. `head_only` is set to 1 when the body must not be sent. */
int handler_reply(const http_parser *parser, http_parse_result parsed,
                  int keep_alive, http_response *res, int *head_only);

/* Standalone error response for cases not coming from the parser
 * (timeout, connection limit). Always closes the connection. */
int handler_error(http_response *res, int status);
