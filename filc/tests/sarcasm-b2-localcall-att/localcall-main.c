#include <stdio.h>

extern long lc_top(long a, long b, long c);
extern long lc_top2(long a, long b, long c);

static long want(long a, long b, long c)
{
    long v = 2 * a + b + c;
    return v < 0 ? -v : v;
}

int main(void)
{
    if (lc_top(10, 3, 4) != 27) {
        printf("FAIL top %ld\n", lc_top(10, 3, 4));
        return 1;
    }
    /* negative sum exercises the jns/neg tail inside the clone */
    if (lc_top(-30, 3, 4) != 53) {
        printf("FAIL neg %ld\n", lc_top(-30, 3, 4));
        return 1;
    }
    /* second jumper: its own region clone with a pre-incremented base
       (the increment happens before the doubling subroutine runs) */
    if (lc_top2(10, 3, 4) != 2027) {
        printf("FAIL top2 %ld\n", lc_top2(10, 3, 4));
        return 1;
    }
    if (lc_top2(-1040, 3, 4) != 73) {
        printf("FAIL neg2 %ld\n", lc_top2(-1040, 3, 4));
        return 1;
    }
    /* the want() model agrees on a spread of values */
    for (long i = -20; i <= 20; i += 7) {
        if (lc_top(i, i * 2, i * 3) != want(i, i * 2, i * 3)) {
            printf("FAIL model %ld\n", i);
            return 1;
        }
    }
    printf("b2 localcall att ok\n");
    return 0;
}
