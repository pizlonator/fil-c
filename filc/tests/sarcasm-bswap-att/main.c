#include <stdio.h>

long f(long x);
long g(long x);
long h(long *p);

int main()
{
    long v = 0x0123456789abcdefL;
    /* f = bswap64(v) = efcdab8967452301; g = bswap32(low32(v)) = 67452301;
       h = bswap64(*p) = efcdab8967452301 */
    printf("%lx %lx %lx\n", f(v), g(v), h(&v));
    /* expect efcdab8967452301 67452301 efcdab8967452301 */
    return 0;
}
