#include <filc_test_support.h>

int foo(int** p, int* r)
{
    int* a = p[0];
    zmemmove_union(p + 2, &r, sizeof(int*));
    int* b = p[1];
    return (a ? *a : 42) + (b ? *b : 666);
}

