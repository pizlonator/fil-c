#include <stdio.h>
#include <stdlib.h>

extern long caps_sum(unsigned long *a, unsigned long *b, long c);
extern long caps_entry(unsigned long *a, unsigned long *b, long c);

int main(void)
{
    unsigned long *a = malloc(32);
    unsigned long *b = malloc(32);
    if (!a || !b)
        return 1;
    a[0] = 1000;
    b[0] = 24;
    /* via the B2 join: 1000 + 24 + 7 = 1031 */
    long r1 = caps_sum(a, b, 7);
    /* via the owner entry: 1000 + 24 + 100 = 1124 */
    long r2 = caps_entry(a, b, 100);
    /* the join must observe the CURRENT memory (not a stale copy) */
    a[0] = 5;
    b[0] = 6;
    long r3 = caps_sum(a, b, 1);
    if (r1 != 1031 || r2 != 1124 || r3 != 12) {
        printf("FAIL: got %ld %ld %ld\n", r1, r2, r3);
        return 1;
    }
    printf("tail b2 caps att ok\n");
    return 0;
}
