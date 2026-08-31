#include "handler.h"

#include <string.h>

#include "files.h"
#include "log.h"

#define LOG_LABEL "handler"
#define WWW_ROOT "www"

static int method_is_supported(const char *method) {
    return strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0;
}

static int method_is_known(const char *method) {
    static const char *known[] = {
        "GET", "HEAD", "POST", "PUT", "DELETE",
        "PATCH", "OPTIONS", "TRACE", "CONNECT",
    };
    size_t i = 0;

    while (i < sizeof(known) / sizeof(known[0])) {
        if (strcmp(method, known[i]) == 0) {
            return 1;
        }
        i++;
    }

    return 0;
}

static int status_for_file_result(file_result result) {
    switch (result) {
    case FILE_OK:         return 200;
    case FILE_NOT_FOUND:  return 404;
    case FILE_FORBIDDEN:  return 403;
    case FILE_TOO_LARGE:  return 413;
    case FILE_BAD_TARGET: return 400;
    }

    return 500;
}

int handler_error(http_response *res, int status) {
    const char *reason = http_status_reason(status);

    return http_response_build(res, status, reason, "text/plain; charset=utf-8",
                               reason, strlen(reason), 0);
}

static int build_ok(const http_request *req, int keep_alive, http_response *res) {
    if (!method_is_known(req->method)) {
        return handler_error(res, 501);
    }
    if (!method_is_supported(req->method)) {
        return handler_error(res, 405);
    }

    file_content content;
    file_result loaded = file_load(WWW_ROOT, req->target, &content);

    if (loaded != FILE_OK) {
        return handler_error(res, status_for_file_result(loaded));
    }

    int rc = http_response_build(res, 200, http_status_reason(200),
                                 content.media_type, content.data,
                                 content.len, keep_alive);
    file_content_free(&content);
    return rc;
}

int handler_reply(const http_parser *parser, http_parse_result parsed,
                  int keep_alive, http_response *res, int *head_only) {
    const http_request *req = http_parser_request(parser);

    *head_only = 0;

    switch (parsed) {
    case HTTP_PARSE_OK:
        *head_only = (strcmp(req->method, "HEAD") == 0);
        log_info("%s %s", req->method, req->target);
        return build_ok(req, keep_alive, res);

    case HTTP_PARSE_TOO_LARGE:
        return handler_error(res, 431);

    default:

        return handler_error(res, 400);
    }
}
