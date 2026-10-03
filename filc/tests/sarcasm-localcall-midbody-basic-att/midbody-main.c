#include <stdio.h>

extern long expand(long *buf, long rounds);

int main(void)
{
    /* rounds 4..1: r9 = 2*r ^ r + 1 = 13, 6, 7, 4, stored in order. */
    long buf[4] = { 0, 0, 0, 0 };
    long r = expand(buf, 4);
    long want[4] = { 13, 6, 7, 4 };
    if (r != 0) {
        printf("FAIL: expand returned %ld, want 0\n", r);
        return 1;
    }
    for (long i = 0; i < 4; i++) {
        if (buf[i] != want[i]) {
            printf("FAIL: buf[%ld] = %ld, want %ld\n", i, buf[i], want[i]);
            return 1;
        }
    }
    printf("localcall midbody basic att ok\n");
    return 0;
}