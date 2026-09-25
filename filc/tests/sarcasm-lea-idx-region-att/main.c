#include <stdio.h>

long leaidx_single(long i);
long leaidx_two(long i);

/* leaidx.s no longer compiles: taking the address of the stack frame is
   rejected at compile time (the D9 fixed-frame escape promotion that used to
   accept the escaping leas was removed). The manifest expects
   `compileFailure`, and the harness compiles every source entry of the test
   and requires each to fail — so this driver fails on purpose alongside the
   rejected .s. */
#error "leaidx.s is rejected at compile time: taking address of stack frame is not supported (the D9 fixed-frame escape promotion was removed)"
int main(void)
{
    for (long i = 0; i < 4; i++) {
        long s = leaidx_single(i);
        long t = leaidx_two(i);
        if (s != 100 + i || t != 100 + i) {
            printf("FAIL i=%ld single=%ld two=%ld\n", i, s, t);
            return 1;
        }
    }
    printf("lea idx region att ok\n");
    return 0;
}
