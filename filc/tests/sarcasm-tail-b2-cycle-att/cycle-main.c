#include <stdio.h>

extern long loop_sum(long n, long base);
extern long loop_owner(long n, long base);

/* base + n + (n-1) + ... + 1 */
static long expect(long n, long base)
{
    long r = base;
    for (long i = n; i > 0; i--)
        r += i;
    return r;
}

int main(void)
{
    /* via the B2 join (loops in the clone) */
    if (loop_sum(5, 100) != expect(5, 100)) {
        printf("FAIL join %ld\n", loop_sum(5, 100));
        return 1;
    }
    /* zero iterations through the join */
    if (loop_sum(0, 7) != 7) {
        printf("FAIL join0\n");
        return 1;
    }
    /* owner entry agrees */
    if (loop_owner(5, 100) != expect(5, 100) || loop_owner(0, 7) != 7) {
        printf("FAIL owner\n");
        return 1;
    }
    if (loop_sum(100, 1) != expect(100, 1)) {
        printf("FAIL join100\n");
        return 1;
    }
    printf("tail b2 cycle att ok\n");
    return 0;
}
