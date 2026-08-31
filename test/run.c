#include <stdio.h>

#include "harness.h"

int checks_run = 0;
int checks_failed = 0;

void test_http_parser(void);
void test_files(void);
void test_http_response(void);

int main(void) {
    printf("\nrunning tests");

    test_http_parser();
    test_files();
    test_http_response();

    printf("\n\n  %d checks, %d failed\n\n",
           checks_run, checks_failed);

    return checks_failed == 0 ? 0 : 1;
}
