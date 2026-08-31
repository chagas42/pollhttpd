#include <string.h>

#include "harness.h"
#include "http_response.h"

void test_http_response(void) {
    TEST("headers and body with correct Content-Length");
    {
        http_response res;
        const char *body = "Hello, World!";

        CHECK_INT(http_response_build(&res, 200, "OK", "text/plain",
                                      body, strlen(body), 0), 0);
        CHECK(strstr(res.data, "HTTP/1.1 200 OK\r\n") == res.data);
        CHECK(strstr(res.data, "Content-Length: 13\r\n") != NULL);
        CHECK(strstr(res.data, "Content-Type: text/plain\r\n") != NULL);
        CHECK(strstr(res.data, "Connection: close\r\n") != NULL);
        CHECK(strstr(res.data, "Date: ") != NULL);
        CHECK_STR(res.data + res.headers_len, "Hello, World!");
        CHECK_INT(res.len, res.headers_len + 13);
        http_response_free(&res);
    }

    TEST("HEAD sends the same headers without a body");
    {
        http_response get, head;
        const char *body = "abcdefghij";

        http_response_build(&get,  200, "OK", "text/plain", body, 10, 0);
        http_response_build(&head, 200, "OK", "text/plain", body, 10, 0);

        CHECK_INT(head.headers_len, get.headers_len);
        CHECK_INT(memcmp(get.data, head.data, head.headers_len), 0);
        CHECK_INT(get.len - get.headers_len, 10);
        http_response_free(&get);
        http_response_free(&head);
    }

    TEST("keep-alive changes the Connection header");
    {
        http_response res;
        http_response_build(&res, 200, "OK", "text/plain", "x", 1, 1);
        CHECK(strstr(res.data, "Connection: keep-alive\r\n") != NULL);
        http_response_free(&res);
    }

    TEST("an empty body is valid");
    {
        http_response res;
        CHECK_INT(http_response_build(&res, 404, "Not Found", "text/plain",
                                      "", 0, 0), 0);
        CHECK(strstr(res.data, "Content-Length: 0\r\n") != NULL);
        CHECK_INT(res.len, res.headers_len);
        http_response_free(&res);
    }

    TEST("reason phrases");
    {
        CHECK_STR(http_status_reason(404), "Not Found");
        CHECK_STR(http_status_reason(501), "Not Implemented");
        CHECK_STR(http_status_reason(999), "Unknown");
    }
}
