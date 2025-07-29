#include <filc_test_support.h>

int foo(int** p, int** q, int* r)
{
    int* a = p[0];
    zmemmove_union(q, &r, sizeof(int*));
    int* b = p[1];
    return (a ? *a : 0) + (b ? *b : 0);
}

