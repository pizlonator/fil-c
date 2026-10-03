#include <stdio.h>

extern long rw_top(long a, long b, long c);
extern long rw_top2(long a, long b, long c);

int main(void)
{
    if (rw_top(7, 10, 3) != 36) {
        printf("FAIL top %ld\n", rw_top(7, 10, 3));
        return 1;
    }
    if (rw_top(0, 1, 1) != 9) {
        printf("FAIL zero %ld\n", rw_top(0, 1, 1));
        return 1;
    }
    if (rw_top(-6, 2, 2) != 0) {
        printf("FAIL neg %ld\n", rw_top(-6, 2, 2));
        return 1;
    }
    if (rw_top(0x7fffffff, 0, 0) != 4) {
        printf("FAIL big %ld\n", rw_top(0x7fffffff, 0, 0));
        return 1;
    }
    if (rw_top2(7, 10, 3) != 46) {
        printf("FAIL top2 %ld\n", rw_top2(7, 10, 3));
        return 1;
    }
    if (rw_top2(-6, 2, 2) != 10) {
        printf("FAIL neg2 %ld\n", rw_top2(-6, 2, 2));
        return 1;
    }
    printf("b2 reg web int ok\n");
    return 0;
}
