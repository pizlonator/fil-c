#include <stdio.h>

extern long c3_top(long a, long b, long c);
extern long c3_top2(long a, long b, long c);

int main(void)
{
    /* b != c: je not taken after three joins -> a + 2*(b+3) + (c+2) */
    if (c3_top(10, 3, 4) != 28) {
        printf("FAIL ne %ld\n", c3_top(10, 3, 4));
        return 1;
    }
    /* b == c: je taken after three joins -> a + 4*(b+3) + (c+2) + 50 */
    if (c3_top(10, 3, 3) != 89) {
        printf("FAIL eq %ld\n", c3_top(10, 3, 3));
        return 1;
    }
    /* direct join into c3_d, both polarities */
    if (c3_top2(10, 3, 4) != 90) {
        printf("FAIL top2 eq %ld\n", c3_top2(10, 3, 4));
        return 1;
    }
    if (c3_top2(10, 3, 9) != 33) {
        printf("FAIL top2 ne %ld\n", c3_top2(10, 3, 9));
        return 1;
    }
    /* negatives */
    if (c3_top(-8, 1, 1) != 61 || c3_top2(-8, 1, 5) != 7) {
        printf("FAIL neg %ld %ld\n", c3_top(-8, 1, 1), c3_top2(-8, 1, 5));
        return 1;
    }
    printf("b2 flags chain3 att ok\n");
    return 0;
}
