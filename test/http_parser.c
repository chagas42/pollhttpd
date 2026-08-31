#include <string.h>

#include "harness.h"
#include "http_parser.h"

void test_http_parser(void) {
    /* ---- the minimal request line ---- */
    TEST("parses a minimal GET request line");
    {
        const char *raw = "GET / HTTP/1.1\r\nHost: x\r\n\r\n";
        http_parser p;
        http_parser_init(&p);

        CHECK_INT(http_parser_feed(&p, raw, strlen(raw)), HTTP_PARSE_OK);
        CHECK_STR(http_parser_request(&p)->method,  "GET");
        CHECK_STR(http_parser_request(&p)->target,  "/");
        CHECK_STR(http_parser_request(&p)->version, "HTTP/1.1");
    }

    /* ---- split at EVERY possible position ---- */
    TEST("same result at every possible split");
    {
        const char *raw = "GET /index.html HTTP/1.1\r\nHost: exemplo\r\n\r\n";
        size_t total = strlen(raw);
        size_t cut = 1;

        while (cut < total) {
            http_parser p;
            http_parser_init(&p);

            http_parse_result first = http_parser_feed(&p, raw, cut);
            http_parse_result second =
                http_parser_feed(&p, raw + cut, total - cut);

            CHECK_INT(first, HTTP_PARSE_INCOMPLETE);
            CHECK_INT(second, HTTP_PARSE_OK);
            CHECK_STR(http_parser_request(&p)->target, "/index.html");
            cut++;
        }
    }

    /* ---- one byte at a time ---- */
    TEST("fed one byte at a time");
    {
        const char *raw = "POST /x HTTP/1.0\r\n\r\n";
        http_parser p;
        http_parser_init(&p);
        http_parse_result r = HTTP_PARSE_INCOMPLETE;
        size_t i = 0;

        while (i < strlen(raw)) {
            r = http_parser_feed(&p, raw + i, 1);
            i++;
        }

        CHECK_INT(r, HTTP_PARSE_OK);
        CHECK_STR(http_parser_request(&p)->method, "POST");
    }

    /* ---- not hardcoded to GET / ---- */
    TEST("any method and any target");
    {
        const char *raw = "DELETE /a/b/c?q=1&r=2 HTTP/1.1\r\nHost: x\r\n\r\n";
        http_parser p;
        http_parser_init(&p);

        CHECK_INT(http_parser_feed(&p, raw, strlen(raw)), HTTP_PARSE_OK);
        CHECK_STR(http_parser_request(&p)->method, "DELETE");
        CHECK_STR(http_parser_request(&p)->target, "/a/b/c?q=1&r=2");
    }

    /* ---- headers, with OWS around the value ---- */
    TEST("headers with optional whitespace");
    {
        const char *raw =
            "GET / HTTP/1.1\r\n"
            "Host: exemplo.com\r\n"
            "User-Agent:    curl/8.5.0   \r\n"
            "X-Vazio:\r\n"
            "\r\n";
        http_parser p;
        http_parser_init(&p);

        CHECK_INT(http_parser_feed(&p, raw, strlen(raw)), HTTP_PARSE_OK);

        const http_request *req = http_parser_request(&p);
        CHECK_INT(req->field_count, 3);
        CHECK_STR(http_request_find_field(req, "Host"), "exemplo.com");
        CHECK_STR(http_request_find_field(req, "User-Agent"), "curl/8.5.0");
        CHECK_STR(http_request_find_field(req, "X-Vazio"), "");
        CHECK_STR(http_request_find_field(req, "hOsT"), "exemplo.com");
        CHECK(http_request_find_field(req, "Ausente") == NULL);
    }

    /* ---- limits, without overrunning a buffer ---- */
    TEST("size limits are enforced");
    {
        static char raw[8192];
        strcpy(raw, "GET /");
        memset(raw + 5, 'a', 4000);
        strcpy(raw + 4005, " HTTP/1.1\r\n\r\n");

        http_parser p;
        http_parser_init(&p);
        CHECK_INT(http_parser_feed(&p, raw, strlen(raw)), HTTP_PARSE_TOO_LARGE);
    }
    {
        static char raw[8192];
        size_t n = 0;
        n += (size_t)sprintf(raw + n, "GET / HTTP/1.1\r\n");
        int i = 0;
        while (i < HTTP_MAX_FIELDS + 5) {
            n += (size_t)sprintf(raw + n, "X-N%d: v\r\n", i);
            i++;
        }
        sprintf(raw + n, "\r\n");

        http_parser p;
        http_parser_init(&p);
        CHECK_INT(http_parser_feed(&p, raw, strlen(raw)), HTTP_PARSE_TOO_LARGE);
    }

    /* ---- malformed input ---- */
    TEST("malformed input yields BAD_REQUEST");
    {
        const char *ruins[] = {
            "GET  / HTTP/1.1\r\n\r\n",      /* two spaces */
            "GET / HTTP/1.1\n\r\n",         /* LF without CR */
            "GET /\r\n\r\n",                /* missing version */
            "GET / HTTP/x.y\r\n\r\n",       /* invalid version */
            "GET / HTTP/1.1\r\n: v\r\n\r\n",/* empty header name */
            " GET / HTTP/1.1\r\n\r\n",      /* empty method */
        };
        size_t i = 0;

        while (i < sizeof(ruins) / sizeof(ruins[0])) {
            http_parser p;
            http_parser_init(&p);
            CHECK_INT(http_parser_feed(&p, ruins[i], strlen(ruins[i])),
                      HTTP_PARSE_BAD_REQUEST);
            i++;
        }
    }

    /* ---- incomplete is not an error ---- */
    TEST("INCOMPLETE is not an error");
    {
        const char *raw = "GET / HTTP/1.1\r\n";   /* no blank line */
        http_parser p;
        http_parser_init(&p);
        CHECK_INT(http_parser_feed(&p, raw, strlen(raw)), HTTP_PARSE_INCOMPLETE);
    }

    /* ---- the result is stable once finished ---- */
    TEST("result is stable after OK");
    {
        const char *raw = "GET / HTTP/1.1\r\nHost: x\r\n\r\n";
        http_parser p;
        http_parser_init(&p);
        http_parser_feed(&p, raw, strlen(raw));
        CHECK_INT(http_parser_feed(&p, "lixo", 4), HTTP_PARSE_OK);
    }

    /* ---- two parsers never mix ---- */
    TEST("state is per parser, not global");
    {
        http_parser a, b;
        http_parser_init(&a);
        http_parser_init(&b);

        http_parser_feed(&a, "GET /a HTTP/1.1\r\n\r\n", 20);
        http_parser_feed(&b, "PUT /b HTTP/1.1\r\n\r\n", 20);

        CHECK_STR(http_parser_request(&a)->target, "/a");
        CHECK_STR(http_parser_request(&b)->target, "/b");
    }

    /* ---------- RFC 9112 conformance ---------- */

    TEST("Host is required on HTTP/1.1 (RFC 9112 3.2)");
    {
        const char *sem = "GET / HTTP/1.1\r\n\r\n";
        const char *dois = "GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n";
        const char *um_zero = "GET / HTTP/1.0\r\n\r\n";
        http_parser p;

        http_parser_init(&p);
        CHECK_INT(http_parser_feed(&p, sem, strlen(sem)), HTTP_PARSE_BAD_REQUEST);

        http_parser_init(&p);
        CHECK_INT(http_parser_feed(&p, dois, strlen(dois)), HTTP_PARSE_BAD_REQUEST);

        http_parser_init(&p);   /* HTTP/1.0 does not require Host */
        CHECK_INT(http_parser_feed(&p, um_zero, strlen(um_zero)), HTTP_PARSE_OK);
    }

    TEST("body framed by Content-Length (RFC 9112 6.3)");
    {
        const char *raw =
            "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\nabcde";
        http_parser p;
        http_parser_init(&p);

        CHECK_INT(http_parser_feed(&p, raw, strlen(raw)), HTTP_PARSE_OK);
        CHECK_INT(http_parser_request(&p)->body_len, 5);
        CHECK_INT(http_parser_request(&p)->body_kind, HTTP_BODY_LENGTH);
    }
    {
        /* an incomplete body stays INCOMPLETE */
        const char *raw =
            "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\nabc";
        http_parser p;
        http_parser_init(&p);
        CHECK_INT(http_parser_feed(&p, raw, strlen(raw)), HTTP_PARSE_INCOMPLETE);
    }
    {
        /* non-numeric Content-Length */
        const char *raw =
            "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: abc\r\n\r\n";
        http_parser p;
        http_parser_init(&p);
        CHECK_INT(http_parser_feed(&p, raw, strlen(raw)), HTTP_PARSE_BAD_REQUEST);
    }

    TEST("chunked body decoding (RFC 9112 7.1)");
    {
        const char *raw =
            "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n"
            "5\r\nhello\r\n"
            "6\r\n world\r\n"
            "0\r\n\r\n";
        http_parser p;
        http_parser_init(&p);

        CHECK_INT(http_parser_feed(&p, raw, strlen(raw)), HTTP_PARSE_OK);
        CHECK_INT(http_parser_request(&p)->body_len, 11);
        CHECK_INT(http_parser_request(&p)->body_kind, HTTP_BODY_CHUNKED);
    }
    {
        /* chunked, byte by byte, gives the same result */
        const char *raw =
            "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n"
            "3\r\nabc\r\n0\r\n\r\n";
        http_parser p;
        http_parser_init(&p);
        http_parse_result r = HTTP_PARSE_INCOMPLETE;
        size_t i = 0;

        while (i < strlen(raw)) {
            r = http_parser_feed(&p, raw + i, 1);
            i++;
        }

        CHECK_INT(r, HTTP_PARSE_OK);
        CHECK_INT(http_parser_request(&p)->body_len, 3);
    }

    TEST("request smuggling: CL and TE together (RFC 9112 6.1)");
    {
        const char *raw =
            "POST / HTTP/1.1\r\nHost: x\r\n"
            "Content-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n";
        http_parser p;
        http_parser_init(&p);

        CHECK_INT(http_parser_feed(&p, raw, strlen(raw)), HTTP_PARSE_CONFLICT);
    }

    TEST("keep-alive decision (RFC 9112 9.3, 9.6)");
    {
        http_parser p;

        http_parser_init(&p);
        http_parser_feed(&p, "GET / HTTP/1.1\r\nHost: x\r\n\r\n", 30);
        CHECK_INT(http_request_wants_keep_alive(http_parser_request(&p)), 1);

        http_parser_init(&p);
        const char *fecha = "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
        http_parser_feed(&p, fecha, strlen(fecha));
        CHECK_INT(http_request_wants_keep_alive(http_parser_request(&p)), 0);

        http_parser_init(&p);
        const char *velho = "GET / HTTP/1.0\r\n\r\n";
        http_parser_feed(&p, velho, strlen(velho));
        CHECK_INT(http_request_wants_keep_alive(http_parser_request(&p)), 0);
    }

    TEST("tells an idle connection from a half-sent request");
    {
        http_parser p;
        http_parser_init(&p);
        CHECK_INT(http_parser_started(&p), 0);

        http_parser_feed(&p, "G", 1);
        CHECK_INT(http_parser_started(&p), 1);
    }
}
