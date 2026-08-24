#include <stdio.h>

/* gcc-style frame with fpOffset != 0 (add x29, sp, #N): x29-relative slots
   at negative and positive offsets, 64-bit and 32-bit, must virtualize
   correctly. Returns a + b + a. */
long fpoff(long a, long b);

int main()
{
    printf("%ld\n", fpoff(10, 20));              /* 40 */
    printf("%ld\n", fpoff(1000000, 2000000));    /* 4000000 */
    return 0;
}
