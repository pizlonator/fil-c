#include <stdio.h>

long f(long *p, long x);

int main()
{
    long v = 123;
    printf("%ld %ld\n", f(&v, 3), f(&v, 9));  /* 1 0 */
    return 0;
}
