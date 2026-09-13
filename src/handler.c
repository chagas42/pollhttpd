#include "handler.h"

#include <string.h>

#include "files.h"
#include "log.h"

#define LOG_LABEL "handler"

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
    case FILE_ERROR:      return 500;
    }

    return 500;
}

int handler_error(http_response *res, int status) {
    const char *reason = http_status_reason(status);

    return http_response_build(res, status, "text/plain; charset=utf-8",
                               reason, strlen(reason), 0);
}

static int build_ok(const http_request *req, bool keep_alive,
                    const char *root, http_response *res) {
    if (!method_is_known(req->method)) {
        return handler_error(res, 501);
    }
    if (!method_is_supported(req->method)) {
        return handler_error(res, 405);
    }

    file_content content;
    file_result loaded = file_load(root, req->target, &content);

    if (loaded != FILE_OK) {
        return handler_error(res, status_for_file_result(loaded));
    }

    int rc = http_response_build(res, 200, content.media_type,
                                 content.data, content.len, keep_alive);
    file_content_free(&content);
    return rc;
}

handler_result handler_reply(const http_request *req, http_parse_result parsed,
                             bool keep_alive, const char *root,
                             http_response *res) {
    handler_result out = { 0, 0 };
    bool head_only = false;

    switch (parsed) {
    case HTTP_PARSE_OK:
        head_only = (strcmp(req->method, "HEAD") == 0);
        log_info("%s %s", req->method, req->target);
        out.ok = build_ok(req, keep_alive, root, res);
        break;

    case HTTP_PARSE_TOO_LARGE:
        out.ok = handler_error(res, 431);
        break;

    default:
        out.ok = handler_error(res, 400);
        break;
    }

    if (out.ok == 0) {
        out.to_send = head_only ? res->headers_len : res->len;
    }

    return out;
}
