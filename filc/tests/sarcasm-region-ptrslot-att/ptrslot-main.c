#include <stdio.h>
#include <stdlib.h>

long region_roundtrip(long *p);

/* ptrslot.s no longer compiles: taking the address of the stack frame is
   rejected at compile time (the D9 fixed-frame escape promotion that used to
   accept the escaping carrier was removed). The manifest expects
   `compileFailure`, and the harness compiles every source entry of the test
   and requires each to fail — so this driver fails on purpose alongside the
   rejected .s. The code below documents what the test used to check. */
#error "ptrslot.s is rejected at compile time: taking address of stack frame is not supported (the D9 fixed-frame escape promotion was removed)"
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