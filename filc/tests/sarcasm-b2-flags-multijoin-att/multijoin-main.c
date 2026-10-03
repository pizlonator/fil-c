#include <stdio.h>

extern long mj_top(long a, long b, long c);
extern long mj_top2(long a, long b, long c);

int main(void)
{
    /* b == c: nested je exit taken, then mj_c's je taken -> a+4b+100 */
    if (mj_top(10, 3, 3) != 122) {
        printf("FAIL top eq %ld\n", mj_top(10, 3, 3));
        return 1;
    }
    /* b != c: mj_b's fallthrough -> a+2b */
    if (mj_top(10, 3, 9) != 16) {
        printf("FAIL top ne %ld\n", mj_top(10, 3, 9));
        return 1;
    }
    /* direct join into mj_c: both polarities there */
    if (mj_top2(10, 3, 3) != 122) {
        printf("FAIL top2 eq %ld\n", mj_top2(10, 3, 3));
        return 1;
    }
    if (mj_top2(10, 3, 9) != 19) {
        printf("FAIL top2 ne %ld\n", mj_top2(10, 3, 9));
        return 1;
    }
    /* negatives */
    if (mj_top(-4, 5, 5) != 116 || mj_top2(-4, 5, 6) != 11) {
        printf("FAIL neg %ld %ld\n", mj_top(-4, 5, 5), mj_top2(-4, 5, 6));
        return 1;
    }
    printf("b2 flags multijoin att ok\n");
    return 0;
}
