#include <stdio.h>

extern long lead0val(long x);

/* lead0val.s no longer compiles: taking the address of the stack frame is
   rejected at compile time (the D9 fixed-frame escape promotion that used to
   accept the value-escaping lea was removed). The manifest expects
   `compileFailure`, and the harness compiles every source entry of the test
   and requires each to fail — so this driver fails on purpose alongside the
   rejected .s. */
#error "lead0val.s is rejected at compile time: taking address of stack frame is not supported (the D9 fixed-frame escape promotion was removed)"
int main(void)
{
    if (lead0val(21) != 201) {
        printf("FAIL d0 value %ld\n", lead0val(21));
        return 1;
    }
    printf("lea carrier d0 value att ok\n");
    return 0;
}
