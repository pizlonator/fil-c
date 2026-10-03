#include <stdio.h>

extern long m5_top(long sel, long a, long b);

int main(void)
{
    /* selector & 0x80108 == 0x80108: je taken -> x4x body */
    if (m5_top(0x80108, 10, 3) != 1083) {
        printf("FAIL taken %ld\n", m5_top(0x80108, 10, 3));
        return 1;
    }
    /* selector & 0x80108 == 0: je not taken -> 4x body */
    if (m5_top(0, 10, 3) != 43) {
        printf("FAIL nottaken %ld\n", m5_top(0, 10, 3));
        return 1;
    }
    /* partial bits set: 0x100 and 0x8 alone still fail the full compare */
    if (m5_top(0x100, 4, 9) != 25 || m5_top(0x8, 4, 9) != 25) {
        printf("FAIL partial %ld %ld\n", m5_top(0x100, 4, 9), m5_top(0x8, 4, 9));
        return 1;
    }
    /* one bit missing from the full mask */
    if (m5_top(0x80100, -6, 11) != -13) {
        printf("FAIL nearmiss %ld\n", m5_top(0x80100, -6, 11));
        return 1;
    }
    /* negatives on both paths */
    if (m5_top(0x80108, -6, 11) != 963 || m5_top(0x10, -6, 11) != -13) {
        printf("FAIL neg %ld %ld\n", m5_top(0x80108, -6, 11), m5_top(0x10, -6, 11));
        return 1;
    }
    printf("mont5 dispatch att ok\n");
    return 0;
}
