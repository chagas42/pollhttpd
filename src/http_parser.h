#pragma once

#include <stddef.h>

#define HTTP_MAX_METHOD       16
#define HTTP_MAX_TARGET     2048
#define HTTP_MAX_VERSION      16
#define HTTP_MAX_FIELD_NAME   64
#define HTTP_MAX_FIELD_VALUE 1024
#define HTTP_MAX_FIELDS       64

typedef enum {
    HTTP_PARSE_INCOMPLETE,
    HTTP_PARSE_OK,
    HTTP_PARSE_BAD_REQUEST,
    HTTP_PARSE_TOO_LARGE,
    HTTP_PARSE_CONFLICT,
} http_parse_result;

typedef enum {
    HTTP_BODY_NONE,
    HTTP_BODY_LENGTH,
    HTTP_BODY_CHUNKED,
} http_body_kind;

typedef struct {
    char name[HTTP_MAX_FIELD_NAME];
    char value[HTTP_MAX_FIELD_VALUE];
} http_field;

typedef struct {
    char       method[HTTP_MAX_METHOD];
    char       target[HTTP_MAX_TARGET];
    char       version[HTTP_MAX_VERSION];
    http_field fields[HTTP_MAX_FIELDS];
    size_t     field_count;

    http_body_kind body_kind;
    size_t         body_len;
} http_request;

typedef struct {

    int          state;
    size_t       fill;
    size_t       remaining;
    http_request request;
} http_parser;

void http_parser_init(http_parser *p);

http_parse_result http_parser_feed(http_parser *p, const char *buf, size_t len);

const http_request *http_parser_request(const http_parser *p);

const char *http_request_find_field(const http_request *req, const char *name);

int http_parser_started(const http_parser *p);

int http_request_wants_keep_alive(const http_request *req);
