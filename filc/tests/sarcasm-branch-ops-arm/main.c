#include <stdio.h>

/* Assorted branch/flag/select/misc ops must survive instrumentation:
   tbz/tbnz bit-test loops, csel/cset, movn, and ldr literal pool loads. */
long popcount(long x);    /* tbz loop: popcount(0xF0F0) = 8 */
long tbnzloop(long x);    /* shifts until bit 63 set: tbnzloop(3) = 62 */
long max2(long a, long b);/* csel: max2(-7, 3) = 3 */
long boolize(long x);     /* cset: 0/1 */
long movnval(void);       /* movn x0, #0 = -1 */
long litval(void);        /* ldr x0, =0x123456789abcdef0 */

int main()
{
    printf("%ld\n", popcount(0xF0F0L));
    printf("%ld\n", tbnzloop(3));
    printf("%ld %ld %ld\n", max2(-7, 3), boolize(99), boolize(0));
    printf("%ld\n", movnval());
    printf("%ld\n", litval());
    return 0;
}
