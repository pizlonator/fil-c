#include <stdio.h>

long caller2(long x);
long caller4(long x);
long caller6(long x);
long callerp(long *p);
long callerpp(long *p);
long callerppp(long *p);

int main()
{
    long v = 63;
    /* 1+8=9, 1+1+1+7=10, 1+1+1+1+1+7=12, 40+63=103, 63+63=126, 63*3=189 */
    printf("%ld %ld %ld %ld %ld %ld\n",
           caller2(1), caller4(1), caller6(1), callerp(&v), callerpp(&v), callerppp(&v));
    return 0;
}
