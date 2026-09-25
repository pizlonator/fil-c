#include <stdio.h>

extern long frameesc(long x);

void fill32(long *p)
{
    for (int i = 0; i < 4; i++) p[i] = 100 + i;
}

/* frameesc.s no longer compiles: taking the address of the stack frame is
   rejected at compile time (the D9 fixed-frame escape promotion that used to
   accept this shape was removed). The manifest expects `compileFailure`, and
   the harness compiles every source entry of the test and requires each to
   fail — so this driver fails on purpose alongside the rejected .s, keeping
   the manifest honest regardless of the directory's source order. The code
   below documents what the test used to check (the helper's writes landing
   in the promoted frame region and the direct slot reads summing them). */
#error "frameesc.s is rejected at compile time: taking address of stack frame is not supported (the D9 fixed-frame escape promotion was removed)"
int main(void)
{
    long r = frameesc(0);
    if (r != 421) {
        printf("FAIL: got %ld, want 421\n", r);
        return 1;
    }
    printf("frame escape region att ok\n");
    return 0;
}
