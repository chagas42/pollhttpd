/* Byte-at-a-time HTTP/1.1 request parser.
 *
 * It consumes bytes, never a descriptor: the parser knows nothing about
 * sockets. Bytes may arrive in any fragmentation — state survives across
 * calls to http_parser_feed(). */
#pragma once

#include <stddef.h>

#define HTTP_MAX_METHOD       16
#define HTTP_MAX_TARGET     2048
#define HTTP_MAX_VERSION      16
#define HTTP_MAX_FIELD_NAME   64
#define HTTP_MAX_FIELD_VALUE 1024
#define HTTP_MAX_FIELDS       64

typedef enum {
    HTTP_PARSE_INCOMPLETE,   /* more bytes needed — not an error */
    HTTP_PARSE_OK,
    HTTP_PARSE_BAD_REQUEST,
    HTTP_PARSE_TOO_LARGE,
    HTTP_PARSE_CONFLICT,     /* Content-Length and Transfer-Encoding together */
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

    /* The body is consumed and discarded: a static file server has no use
     * for the content, but MUST drain it so the remainder is not read as
     * the next request (RFC 9112 9.3). */
    http_body_kind body_kind;
    size_t         body_len;
} http_request;

typedef struct {
    /* private — do not touch outside the parser */
    int          state;
    size_t       fill;
    size_t       remaining;
    http_request request;
} http_parser;

/* Prepares the parser. Call once per connection. */
void http_parser_init(http_parser *p);

/* Consumes `len` bytes. Returns INCOMPLETE while input is still missing.
 * After OK or an error, later calls return the same result. */
http_parse_result http_parser_feed(http_parser *p, const char *buf, size_t len);

/* The accumulated request. Only meaningful after OK. */
const http_request *http_parser_request(const http_parser *p);

/* Finds a header by name, case-insensitively.
 * Returns the value, or NULL if absent. */
const char *http_request_find_field(const http_request *req, const char *name);

/* 1 if any byte has been consumed — tells an idle connection apart from
 * a half-sent request. */
int http_parser_started(const http_parser *p);

/* 1 if the connection should survive this request (RFC 9112 9.3). */
int http_request_wants_keep_alive(const http_request *req);
