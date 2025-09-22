
#ifndef PROCPS_NG_TESTS_H
#define PROCPS_NG_TESTS_H

#include <stdio.h>

typedef int (*TestFunction)(void *data);

char *testname;


static inline int run_tests(TestFunction *list, void *data)
{
    int i;
    TestFunction current;

    for (i=0; list[i] != NULL; i++) {
        testname = NULL;
        current = list[i];
        if (!current(data)) {
            fprintf(stderr, "FAIL: %s\n", testname);
            return EXIT_FAILURE;
        } else {
            fprintf(stderr, "PASS: %s\n", testname);
        }
    }
    return EXIT_SUCCESS;
}
#endif
