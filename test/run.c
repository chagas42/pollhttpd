/* Test runner. Each suite is a function declared here and called below;
 * the exit code reports whether everything passed. */

#include <stdio.h>

#include "harness.h"

int checks_run = 0;
int checks_failed = 0;

void test_http_parser(void);   /* Milestone 3 */
void test_files(void);         /* Milestone 4 */
void test_http_response(void); /* Milestone 4 */

int main(void) {
    printf("\nrunning tests");

    test_http_parser();
    test_files();
    test_http_response();

    printf("\n\n  %d checks, %d failed\n\n",
           checks_run, checks_failed);

    return checks_failed == 0 ? 0 : 1;
}
