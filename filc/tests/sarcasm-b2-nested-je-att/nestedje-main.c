#include <stdio.h>

extern long cn_top(long a, long b, long c);

int main(void)
{
    /* b != c: cn_b fallthrough a + 2b */
    if (cn_top(10, 3, 4) != 16) {
        printf("FAIL ne %ld\n", cn_top(10, 3, 4));
        return 1;
    }
    /* b == c, a != b, b != 11: cn_c's tail a + b */
    if (cn_top(10, 3, 3) != 13) {
        printf("FAIL tail %ld\n", cn_top(10, 3, 3));
        return 1;
    }
    /* b == c == a: second nested je exit -> cn_d: 4c + a - b */
    if (cn_top(7, 7, 7) != 28) {
        printf("FAIL d %ld\n", cn_top(7, 7, 7));
        return 1;
    }
    /* b == c, b != a, b == 11: conditional B1 exit -> cn_e: a + 77 + c */
    if (cn_top(5, 11, 11) != 93) {
        printf("FAIL e %ld\n", cn_top(5, 11, 11));
        return 1;
    }
    /* negatives through the nested exits */
    if (cn_top(-3, -3, -3) != -12) {
        printf("FAIL neg d %ld\n", cn_top(-3, -3, -3));
        return 1;
    }
    printf("b2 nested je att ok\n");
    return 0;
}
