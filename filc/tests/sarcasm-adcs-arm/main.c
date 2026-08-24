#include <stdio.h>

long f(long *p, long x, long y);
long g(long *p, long x, long y);

int main()
{
    long v = 100;
    /* f = x + y + carry(x >=u y) + (v + 1):  5+3+1+101=110, 3+5+0+101=109
       g = x - y - 1 + carry(x >=u y) + (v + 1):  5-3-1+1+101=103, 3-5-1+0+101=98 */
    printf("%ld %ld %ld %ld\n", f(&v, 5, 3), f(&v, 3, 5), g(&v, 5, 3), g(&v, 3, 5));
    /* expect 110 109 103 98 */
    return 0;
}
