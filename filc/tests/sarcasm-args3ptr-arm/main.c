#include <stdio.h>

/* long(ptr, ptr, ptr) is exactly six dense argument words at the fast
   entrypoint (three capabilities x 2 words) -- the boundary case. */
long sum3(long* a, long* b, long* c);

int main()
{
    long a = 10, b = 20, c = 30;
    printf("%ld\n", sum3(&a, &b, &c));   /* 60 */
    return 0;
}
