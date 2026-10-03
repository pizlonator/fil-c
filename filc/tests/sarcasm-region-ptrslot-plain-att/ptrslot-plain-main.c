#include <stdio.h>
#include <stdlib.h>

long region_roundtrip_plain(long *p);

int main(void)
{
    long *p = malloc(128);
    if (!p)
        return 1;
    p[0] = 4242;
    /* the assembly must trap before returning; reaching the printf below
       means the capability loss was NOT caught */
    long r = region_roundtrip_plain(p);
    printf("FAIL: returned %ld instead of trapping\n", r);
    return 1;
}