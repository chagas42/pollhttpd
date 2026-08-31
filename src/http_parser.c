#include "http_parser.h"

#include <string.h>
#include <strings.h>

enum {
    S_METHOD,
    S_TARGET,
    S_VERSION,
    S_REQLINE_LF,
    S_FIELD_START,
    S_FIELD_NAME,
    S_VALUE_WS,
    S_VALUE,
    S_FIELD_LF,
    S_END_LF,
    S_BODY,
    S_CHUNK_SIZE,
    S_CHUNK_SIZE_LF,
    S_CHUNK_DATA,
    S_CHUNK_DATA_CR,
    S_CHUNK_DATA_LF,
    S_TRAILER_START,
    S_TRAILER_LINE,
    S_TRAILER_LINE_LF,
    S_TRAILER_END_LF,
    S_DONE,
    S_BAD_REQUEST,
    S_TOO_LARGE,
    S_CONFLICT,
};

#define HTTP_MAX_BODY (1024 * 1024)

#define CR '\r'
#define LF '\n'

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int is_token_char(char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9')) {
        return 1;
    }

    return c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
           c == '\'' || c == '*' || c == '+' || c == '-' || c == '.' ||
           c == '^' || c == '_' || c == '`' || c == '|' || c == '~';
}

static int push(char *dst, size_t cap, size_t *fill, char c) {
    if (*fill + 1 >= cap) {
        return -1;
    }

    dst[*fill] = c;
    *fill += 1;
    dst[*fill] = '\0';
    return 0;
}

static int is_valid_version(const char *v) {
    return v[0] == 'H' && v[1] == 'T' && v[2] == 'T' && v[3] == 'P' &&
           v[4] == '/' && v[5] >= '0' && v[5] <= '9' &&
           v[6] == '.' && v[7] >= '0' && v[7] <= '9' && v[8] == '\0';
}

static http_field *current_field(http_parser *p) {
    return &p->request.fields[p->request.field_count];
}

static void trim_trailing_ws(char *value, size_t *len) {
    while (*len > 0 && (value[*len - 1] == ' ' || value[*len - 1] == '\t')) {
        *len -= 1;
        value[*len] = '\0';
    }
}

static int host_is_valid(const http_request *req) {
    size_t seen = 0;
    size_t i = 0;

    while (i < req->field_count) {
        if (strcasecmp(req->fields[i].name, "Host") == 0) {
            seen += 1;
        }
        i++;
    }

    if (strcmp(req->version, "HTTP/1.1") != 0) {
        return 1;
    }

    return seen == 1;
}

static void begin_body(http_parser *p) {
    const http_request *req = &p->request;

    if (!host_is_valid(req)) {
        p->state = S_BAD_REQUEST;
        return;
    }

    const char *length = http_request_find_field(req, "Content-Length");
    const char *encoding = http_request_find_field(req, "Transfer-Encoding");

    if (length != NULL && encoding != NULL) {
        p->state = S_CONFLICT;
        return;
    }

    if (encoding != NULL) {
        if (strcasecmp(encoding, "chunked") != 0) {
            p->state = S_BAD_REQUEST;
            return;
        }
        p->request.body_kind = HTTP_BODY_CHUNKED;
        p->remaining = 0;
        p->fill = 0;
        p->state = S_CHUNK_SIZE;
        return;
    }

    if (length != NULL) {
        size_t value = 0;
        const char *d = length;

        if (*d == '\0') {
            p->state = S_BAD_REQUEST;
            return;
        }

        while (*d != '\0') {
            if (*d < '0' || *d > '9') {
                p->state = S_BAD_REQUEST;
                return;
            }
            value = value * 10 + (size_t)(*d - '0');
            if (value > HTTP_MAX_BODY) {
                p->state = S_TOO_LARGE;
                return;
            }
            d++;
        }

        p->request.body_kind = HTTP_BODY_LENGTH;
        p->remaining = value;
        p->state = (value == 0) ? S_DONE : S_BODY;
        return;
    }

    p->request.body_kind = HTTP_BODY_NONE;
    p->state = S_DONE;
}

void http_parser_init(http_parser *p) {
    p->state = S_METHOD;
    p->fill = 0;
    p->remaining = 0;
    p->request.body_kind = HTTP_BODY_NONE;
    p->request.body_len = 0;
    p->request.method[0] = '\0';
    p->request.target[0] = '\0';
    p->request.version[0] = '\0';
    p->request.field_count = 0;
}

static void step(http_parser *p, char c) {
    switch (p->state) {

    case S_METHOD:
        if (c == ' ') {
            if (p->fill == 0) {
                p->state = S_BAD_REQUEST;
                return;
            }
            p->fill = 0;
            p->state = S_TARGET;
            return;
        }
        if (!is_token_char(c)) {
            p->state = S_BAD_REQUEST;
            return;
        }
        if (push(p->request.method, HTTP_MAX_METHOD, &p->fill, c) == -1) {
            p->state = S_TOO_LARGE;
        }
        return;

    case S_TARGET:
        if (c == ' ') {
            if (p->fill == 0) {
                p->state = S_BAD_REQUEST;
                return;
            }
            p->fill = 0;
            p->state = S_VERSION;
            return;
        }
        if (c == CR || c == LF || (unsigned char)c < 0x20) {
            p->state = S_BAD_REQUEST;
            return;
        }
        if (push(p->request.target, HTTP_MAX_TARGET, &p->fill, c) == -1) {
            p->state = S_TOO_LARGE;
        }
        return;

    case S_VERSION:
        if (c == CR) {
            if (!is_valid_version(p->request.version)) {
                p->state = S_BAD_REQUEST;
                return;
            }
            p->fill = 0;
            p->state = S_REQLINE_LF;
            return;
        }
        if (push(p->request.version, HTTP_MAX_VERSION, &p->fill, c) == -1) {
            p->state = S_TOO_LARGE;
        }
        return;

    case S_REQLINE_LF:
        p->state = (c == LF) ? S_FIELD_START : S_BAD_REQUEST;
        return;

    case S_FIELD_START:
        if (c == CR) {
            p->state = S_END_LF;
            return;
        }
        if (!is_token_char(c)) {
            p->state = S_BAD_REQUEST;
            return;
        }
        if (p->request.field_count >= HTTP_MAX_FIELDS) {
            p->state = S_TOO_LARGE;
            return;
        }
        current_field(p)->name[0] = '\0';
        current_field(p)->value[0] = '\0';
        p->fill = 0;
        push(current_field(p)->name, HTTP_MAX_FIELD_NAME, &p->fill, c);
        p->state = S_FIELD_NAME;
        return;

    case S_FIELD_NAME:
        if (c == ':') {
            p->fill = 0;
            p->state = S_VALUE_WS;
            return;
        }
        if (!is_token_char(c)) {
            p->state = S_BAD_REQUEST;
            return;
        }
        if (push(current_field(p)->name, HTTP_MAX_FIELD_NAME, &p->fill, c) == -1) {
            p->state = S_TOO_LARGE;
        }
        return;

    case S_VALUE_WS:
        if (c == ' ' || c == '\t') {
            return;
        }
        if (c == CR) {
            p->state = S_FIELD_LF;
            return;
        }
        if (push(current_field(p)->value, HTTP_MAX_FIELD_VALUE, &p->fill, c) == -1) {
            p->state = S_TOO_LARGE;
            return;
        }
        p->state = S_VALUE;
        return;

    case S_VALUE:
        if (c == CR) {
            trim_trailing_ws(current_field(p)->value, &p->fill);
            p->state = S_FIELD_LF;
            return;
        }
        if (push(current_field(p)->value, HTTP_MAX_FIELD_VALUE, &p->fill, c) == -1) {
            p->state = S_TOO_LARGE;
        }
        return;

    case S_FIELD_LF:
        if (c != LF) {
            p->state = S_BAD_REQUEST;
            return;
        }
        p->request.field_count += 1;
        p->fill = 0;
        p->state = S_FIELD_START;
        return;

    case S_END_LF:
        if (c != LF) {
            p->state = S_BAD_REQUEST;
            return;
        }
        begin_body(p);
        return;

    case S_BODY:
        p->request.body_len += 1;
        p->remaining -= 1;
        if (p->remaining == 0) {
            p->state = S_DONE;
        }
        return;

    case S_CHUNK_SIZE:
        if (c == CR) {
            if (p->fill == 0) {
                p->state = S_BAD_REQUEST;
                return;
            }
            p->state = S_CHUNK_SIZE_LF;
            return;
        }
        if (c == ';') {
            return;
        }
        {
            int digit = hex_value(c);
            if (digit < 0) {
                p->state = S_BAD_REQUEST;
                return;
            }
            if (p->remaining > HTTP_MAX_BODY / 16) {
                p->state = S_TOO_LARGE;
                return;
            }
            p->remaining = p->remaining * 16 + (size_t)digit;
            p->fill += 1;
        }
        return;

    case S_CHUNK_SIZE_LF:
        if (c != LF) {
            p->state = S_BAD_REQUEST;
            return;
        }
        p->fill = 0;
        p->state = (p->remaining == 0) ? S_TRAILER_START : S_CHUNK_DATA;
        return;

    case S_CHUNK_DATA:
        p->request.body_len += 1;
        p->remaining -= 1;
        if (p->remaining == 0) {
            p->state = S_CHUNK_DATA_CR;
        }
        return;

    case S_CHUNK_DATA_CR:
        p->state = (c == CR) ? S_CHUNK_DATA_LF : S_BAD_REQUEST;
        return;

    case S_CHUNK_DATA_LF:
        p->state = (c == LF) ? S_CHUNK_SIZE : S_BAD_REQUEST;
        return;

    case S_TRAILER_START:
        p->state = (c == CR) ? S_TRAILER_END_LF : S_TRAILER_LINE;
        return;

    case S_TRAILER_LINE:
        if (c == CR) {
            p->state = S_TRAILER_LINE_LF;
        }
        return;

    case S_TRAILER_LINE_LF:
        p->state = (c == LF) ? S_TRAILER_START : S_BAD_REQUEST;
        return;

    case S_TRAILER_END_LF:
        p->state = (c == LF) ? S_DONE : S_BAD_REQUEST;
        return;

    default:
        return;
    }
}

http_parse_result http_parser_feed(http_parser *p, const char *buf, size_t len) {
    size_t i = 0;

    while (i < len && p->state != S_DONE && p->state != S_BAD_REQUEST &&
           p->state != S_TOO_LARGE && p->state != S_CONFLICT) {
        step(p, buf[i]);
        i++;
    }

    if (p->state == S_DONE) {
        return HTTP_PARSE_OK;
    }
    if (p->state == S_BAD_REQUEST) {
        return HTTP_PARSE_BAD_REQUEST;
    }
    if (p->state == S_TOO_LARGE) {
        return HTTP_PARSE_TOO_LARGE;
    }
    if (p->state == S_CONFLICT) {
        return HTTP_PARSE_CONFLICT;
    }

    return HTTP_PARSE_INCOMPLETE;
}

int http_parser_started(const http_parser *p) {
    return !(p->state == S_METHOD && p->fill == 0);
}

int http_request_wants_keep_alive(const http_request *req) {
    const char *connection = http_request_find_field(req, "Connection");

    if (connection != NULL && strcasecmp(connection, "close") == 0) {
        return 0;
    }

    if (strcmp(req->version, "HTTP/1.0") == 0) {
        return connection != NULL && strcasecmp(connection, "keep-alive") == 0;
    }

    return 1;
}

const http_request *http_parser_request(const http_parser *p) {
    return &p->request;
}

const char *http_request_find_field(const http_request *req, const char *name) {
    size_t i = 0;

    while (i < req->field_count) {
        if (strcasecmp(req->fields[i].name, name) == 0) {
            return req->fields[i].value;
        }
        i++;
    }

    return NULL;
}
