#include <stdio.h>

/* stnp/ldnp on sp- and x29-relative frame slots must round-trip through the
   virtualized slot webs; ldpsw must sign-extend BOTH 32-bit halves (values
   with bit 31 set come back negative). */
long np_pair(long a, long b);
long swsum(long a, long b);
long x29np(long a, long b);
long x29sw(long a, long b);

int main()
{
    printf("%ld\n", np_pair(40, 2));           /* 42 */
    /* w halves: 0x80000001 -> -2147483647, 0xfffffffb -> -5 */
    printf("%ld\n", swsum(0x80000001L, -5));   /* -2147483652 */
    printf("%ld\n", x29np(100, 23));           /* 123 */
    /* w halves: 0xffffffff -> -1, 0x80000000 -> -2147483648 */
    printf("%ld\n", x29sw(-1L, 0x80000000L));  /* -2147483649 */
    return 0;
}
