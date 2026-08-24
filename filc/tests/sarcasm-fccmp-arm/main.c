#include <stdio.h>

long f(long *p, long x);

int main()
{
    long v = 55;
    printf("%ld %ld\n", f(&v, 5), f(&v, 7));    /* 1 0 */
    return 0;
}
