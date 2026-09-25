#include <stdio.h>
#include <stdlib.h>

long region_roundtrip_plain(long *p);

/* ptrslot-plain.s no longer compiles: taking the address of the stack frame
   is rejected at compile time (the D9 fixed-frame escape promotion that used
   to accept the escaping carrier — and whose region memory made the plain
   pointer round-trip trap with "cannot read pointer with null object" — was
   removed). The manifest expects `compileFailure`, and the harness compiles
   every source entry of the test and requires each to fail — so this driver
   fails on purpose alongside the rejected .s. */
#error "ptrslot-plain.s is rejected at compile time: taking address of stack frame is not supported (the D9 fixed-frame escape promotion was removed)"
int main(void)
{
    long *p = malloc(128);
    if (!p)
        return 1;
    p[0] = 4242;
    long r = region_roundtrip_plain(p);
    printf("FAIL: returned %ld instead of trapping\n", r);
    return 1;
}