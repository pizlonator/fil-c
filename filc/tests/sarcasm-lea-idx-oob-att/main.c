#include <stdio.h>

long leaidx_oob(long i);

/* leaidxoob.s no longer compiles: taking the address of the stack frame is
   rejected at compile time (the D9 fixed-frame escape promotion that used to
   accept the escaping lea — and whose runtime bounds check used to trap on
   the out-of-region index — was removed). The manifest expects
   `compileFailure`, and the harness compiles every source entry of the test
   and requires each to fail — so this driver fails on purpose alongside the
   rejected .s. */
#error "leaidxoob.s is rejected at compile time: taking address of stack frame is not supported (the D9 fixed-frame escape promotion was removed)"
int main(void)
{
    printf("expect trap:\n");
    printf("%ld SHOULD NOT PRINT\n", leaidx_oob(100));
    return 0;
}
