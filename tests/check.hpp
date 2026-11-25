// Minimal test harness; independent of NDEBUG.
#pragma once
#include <cstdio>
#include <cstdlib>

static int g_checks = 0, g_fails = 0;
#define CHECK(cond)                                                            \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            ++g_fails;                                                         \
            if (g_fails <= 20) std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                      \
    } while (0)
#define TEST_MAIN_END()                                                        \
    do {                                                                       \
        std::printf("%d checks, %d failures\n", g_checks, g_fails);            \
        return g_fails ? EXIT_FAILURE : EXIT_SUCCESS;                          \
    } while (0)
