/* Minimal assertions for the test suite.
 *
 * Each CHECK reports the failure with file and line and KEEPS GOING: one
 * run shows everything that broke, not just the first thing. */
#pragma once

#include <stdio.h>
#include <string.h>

extern int checks_run;
extern int checks_failed;

#define TEST(name) printf("\n  %s\n", (name))

#define CHECK(cond)                                                        \
    do {                                                                   \
        checks_run++;                                                      \
        if (!(cond)) {                                                     \
            checks_failed++;                                               \
            printf("    FALHOU  %s:%d\n      %s\n",                        \
                   __FILE__, __LINE__, #cond);                             \
        }                                                                  \
    } while (0)

#define CHECK_INT(got, want)                                               \
    do {                                                                   \
        checks_run++;                                                      \
        long _g = (long)(got), _w = (long)(want);                          \
        if (_g != _w) {                                                    \
            checks_failed++;                                               \
            printf("    FALHOU  %s:%d\n      esperado: %ld\n"              \
                   "      obtido:   %ld\n", __FILE__, __LINE__, _w, _g);   \
        }                                                                  \
    } while (0)

#define CHECK_STR(got, want)                                               \
    do {                                                                   \
        checks_run++;                                                      \
        const char *_g = (got);                                            \
        const char *_w = (want);                                           \
        if (_g == NULL || strcmp(_g, _w) != 0) {                           \
            checks_failed++;                                               \
            printf("    FALHOU  %s:%d\n      esperado: \"%s\"\n"           \
                   "      obtido:   \"%s\"\n", __FILE__, __LINE__,         \
                   _w, _g ? _g : "(null)");                                \
        }                                                                  \
    } while (0)
