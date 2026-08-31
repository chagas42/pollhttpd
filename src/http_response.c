#include "http_response.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void format_http_date(char *out, size_t out_size) {
    time_t now = time(NULL);
    struct tm gmt;

    gmtime_r(&now, &gmt);
    strftime(out, out_size, "%a, %d %b %Y %H:%M:%S GMT", &gmt);
}

const char *http_status_reason(int status) {
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 413: return "Content Too Large";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    default:  return "Unknown";
    }
}

int http_response_build(http_response *res, int status, const char *reason,
                        const char *media_type, const char *body,
                        size_t body_len, int keep_alive) {
    char date[64];
    format_http_date(date, sizeof(date));

    char headers[512];
    int headers_len = snprintf(headers, sizeof(headers),
                               "HTTP/1.1 %d %s\r\n"
                               "Date: %s\r\n"
                               "Content-Type: %s\r\n"
                               "Content-Length: %zu\r\n"
                               "Connection: %s\r\n"
                               "\r\n",
                               status, reason, date, media_type, body_len,
                               keep_alive ? "keep-alive" : "close");

    if (headers_len < 0 || (size_t)headers_len >= sizeof(headers)) {
        return -1;
    }

    char *buffer = malloc((size_t)headers_len + body_len + 1);
    if (buffer == NULL) {
        return -1;
    }

    memcpy(buffer, headers, (size_t)headers_len);
    if (body_len > 0) {
        memcpy(buffer + headers_len, body, body_len);
    }
    buffer[(size_t)headers_len + body_len] = '\0';

    res->data = buffer;
    res->len = (size_t)headers_len + body_len;
    res->headers_len = (size_t)headers_len;
    return 0;
}

void http_response_free(http_response *res) {
    free(res->data);
    res->data = NULL;
    res->len = 0;
    res->headers_len = 0;
}
