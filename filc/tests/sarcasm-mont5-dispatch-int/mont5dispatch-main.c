#include <stdio.h>

extern long m5_top(long sel, long a, long b);

int main(void)
{
    if (m5_top(0x80108, 10, 3) != 1083) {
        printf("FAIL taken %ld\n", m5_top(0x80108, 10, 3));
        return 1;
    }
    if (m5_top(0, 10, 3) != 43) {
        printf("FAIL nottaken %ld\n", m5_top(0, 10, 3));
        return 1;
    }
    if (m5_top(0x100, 4, 9) != 25 || m5_top(0x8, 4, 9) != 25) {
        printf("FAIL partial %ld %ld\n", m5_top(0x100, 4, 9), m5_top(0x8, 4, 9));
        return 1;
    }
    if (m5_top(0x80100, -6, 11) != -13) {
        printf("FAIL nearmiss %ld\n", m5_top(0x80100, -6, 11));
        return 1;
    }
    if (m5_top(0x80108, -6, 11) != 963 || m5_top(0x10, -6, 11) != -13) {
        printf("FAIL neg %ld %ld\n", m5_top(0x80108, -6, 11), m5_top(0x10, -6, 11));
        return 1;
    }
    printf("mont5 dispatch int ok\n");
    return 0;
}
