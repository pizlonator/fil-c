#include <stdio.h>
#include <stdlib.h>

long region_roundtrip(long *p);

int main(void)
{
    long *p = malloc(128);
    if (!p)
        return 1;
    p[0] = 4242;
    p[8] = 777;
    long r = region_roundtrip(p);
    if (r != 4242 + 2 + 777) {
        printf("FAIL: region_roundtrip got %ld, want %d\n", r, 4242 + 2 + 777);
        return 1;
    }
    printf("region ptrslot ok\n");
    return 0;
}