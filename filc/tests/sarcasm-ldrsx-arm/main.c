#include <stdio.h>

/* Sign-extended loads must sign-extend, and [x0, w1, uxtw #N] extended-index
   addressing must zero-extend the 32-bit index before scaling. */
long loadb(const void* p, long i);   /* ldrsb */
long loadh(const void* p, long i);   /* ldrsh */
long loadw(const void* p, long i);   /* ldrsw */
long loadu(const void* p, int i);    /* ldr w0, [x0, w1, uxtw #2] */
long loadx(const void* p, int i);    /* ldr x0, [x0, w1, uxtw #3] */

int main()
{
    static signed char b[] = { -5, 100, -128, 0, 127 };
    static short h[] = { -300, 20000, -32768, 12345 };
    static int w[] = { -70000, 100000, -2147483647 - 1, 42 };
    static unsigned int u[] = { 4000000000u, 123456789u };
    static long v[] = { 111111111111L, 222222222222L, 333333333333L };
    printf("%ld %ld %ld\n", loadb(b, 0), loadb(b, 2), loadb(b, 4));
    printf("%ld %ld %ld\n", loadh(h, 0), loadh(h, 2), loadh(h, 3));
    printf("%ld %ld %ld\n", loadw(w, 0), loadw(w, 2), loadw(w, 3));
    /* index is a 32-bit int: uxtw must treat w1 as unsigned */
    printf("%ld %ld\n", loadu(u, 0), loadu(u, 1));
    printf("%ld %ld\n", loadx(v, 1), loadx(v, 2));
    return 0;
}
