#include <stdio.h>
long lcspill_main(long a0, long a1, long a2, long a3, long a4, long a5,
                  long a6, long a7, long a8, long a9, long a10, long a11);
int main() {
    long a[12] = {3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41};
    long w[12] = {3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 43};
    long sub = 0, sum10 = 0;
    for (int i = 0; i < 12; ++i) sub += a[i] * w[i];
    for (int i = 0; i < 11; ++i) sum10 += a[i];
    // result = sub1 + sub2 + sum(a0..a10) + 2*a11 (see lcspill.s).
    long expect = 2 * sub + sum10 + 2 * a[11];
    long got = lcspill_main(a[0], a[1], a[2], a[3], a[4], a[5],
                            a[6], a[7], a[8], a[9], a[10], a[11]);
    if (got == expect) printf("lcspill ok %ld\n", got);
    else printf("lcspill BAD %ld %ld\n", got, expect);
    return 0;
}
