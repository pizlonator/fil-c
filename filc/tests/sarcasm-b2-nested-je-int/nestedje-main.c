#include <stdio.h>

extern long cn_top(long a, long b, long c);

int main(void)
{
    if (cn_top(10, 3, 4) != 16) {
        printf("FAIL ne %ld\n", cn_top(10, 3, 4));
        return 1;
    }
    if (cn_top(10, 3, 3) != 13) {
        printf("FAIL tail %ld\n", cn_top(10, 3, 3));
        return 1;
    }
    if (cn_top(7, 7, 7) != 28) {
        printf("FAIL d %ld\n", cn_top(7, 7, 7));
        return 1;
    }
    if (cn_top(5, 11, 11) != 93) {
        printf("FAIL e %ld\n", cn_top(5, 11, 11));
        return 1;
    }
    if (cn_top(-3, -3, -3) != -12) {
        printf("FAIL neg d %ld\n", cn_top(-3, -3, -3));
        return 1;
    }
    printf("b2 nested je int ok\n");
    return 0;
}
