#include <stdio.h>
#include <stdlib.h>

extern void mul_ptr(long *out, long *a, long *b);
extern void sqr_ptr(long *out, long *a);

int main(void)
{
    long *o = malloc(32), *a = malloc(32), *b = malloc(32);
    if (!o || !a || !b)
        return 2;
    a[0] = 11;
    a[1] = 22;
    b[0] = 33;
    b[1] = 44;
    /* r8 = a[0]+b[0] = 44; tail: r9 = b[0]+r8 = 77. */
    mul_ptr(o, a, b);
    if (o[0] != 44 || o[1] != 77) {
        printf("FAIL mul_ptr: got %ld %ld, want 44 77\n", o[0], o[1]);
        return 1;
    }
    /* r8 = a[0]+a[1] = 33; tail: r9 = a[1]+r8 = 55. */
    sqr_ptr(o, a);
    if (o[0] != 33 || o[1] != 55) {
        printf("FAIL sqr_ptr: got %ld %ld, want 33 55\n", o[0], o[1]);
        return 1;
    }
    /* Again through the join, to catch clobbered callee-saved restores.
       r8 = b[0]+a[0] = 44; tail: r9 = a[0]+r8 = 55. */
    mul_ptr(o, b, a);
    if (o[0] != 44 || o[1] != 55) {
        printf("FAIL mul_ptr again: got %ld %ld, want 44 55\n", o[0], o[1]);
        return 1;
    }
    printf("sharedtail ptr att ok\n");
    return 0;
}
