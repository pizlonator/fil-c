#include <stdio.h>

extern long rw_top(long a, long b, long c);
extern long rw_top2(long a, long b, long c);

static long want(long a, long b, long c, long pre)
{
    long r12 = b + 10 * pre + 1;   /* pre==1: rw_top2 adds 10 before joining */
    long r11 = (int)(a + 5);       /* 32-bit addl, then sign-extended */
    return r12 + 2 * c + r11 + a;
}

int main(void)
{
    if (rw_top(7, 10, 3) != 36) {   /* 11 + 6 + 12 + 7 */
        printf("FAIL top %ld\n", rw_top(7, 10, 3));
        return 1;
    }
    if (rw_top(0, 1, 1) != 9) {     /* 2 + 2 + 5 + 0 */
        printf("FAIL zero %ld\n", rw_top(0, 1, 1));
        return 1;
    }
    /* negative a exercises the sign-extending movslq of the partial web */
    if (rw_top(-6, 2, 2) != 0) {    /* 3 + 4 + (-1) + (-6) */
        printf("FAIL neg %ld\n", rw_top(-6, 2, 2));
        return 1;
    }
    if (rw_top(0x7fffffff, 0, 0) != want(0x7fffffff, 0, 0, 0)) {
        printf("FAIL big %ld\n", rw_top(0x7fffffff, 0, 0));
        return 1;
    }
    /* second jumper: %r12 pre-incremented by the caller */
    if (rw_top2(7, 10, 3) != 46) {  /* 21 + 6 + 12 + 7 */
        printf("FAIL top2 %ld\n", rw_top2(7, 10, 3));
        return 1;
    }
    if (rw_top2(-6, 2, 2) != 10) {  /* 13 + 4 + (-1) + (-6) */
        printf("FAIL neg2 %ld\n", rw_top2(-6, 2, 2));
        return 1;
    }
    printf("b2 reg web att ok\n");
    return 0;
}
