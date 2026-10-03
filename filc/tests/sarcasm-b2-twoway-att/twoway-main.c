#include <stdio.h>

extern long tw_top(long which, long a, long b);

int main(void)
{
    /* which == 0: first site (je) -> 3a + b */
    if (tw_top(0, 5, 7) != 22) {
        printf("FAIL site1 %ld\n", tw_top(0, 5, 7));
        return 1;
    }
    /* which != 0: second site (jmp) -> 3a + 100 + b */
    if (tw_top(1, 5, 7) != 122) {
        printf("FAIL site2 %ld\n", tw_top(1, 5, 7));
        return 1;
    }
    if (tw_top(-2, -3, 4) != 95) {
        printf("FAIL neg2 %ld\n", tw_top(-2, -3, 4));
        return 1;
    }
    if (tw_top(0, -3, 4) != -5) {
        printf("FAIL neg1 %ld\n", tw_top(0, -3, 4));
        return 1;
    }
    if (tw_top(0, -7, 2) != -19) {
        printf("FAIL neg3 %ld\n", tw_top(0, -7, 2));
        return 1;
    }
    printf("b2 twoway att ok\n");
    return 0;
}
