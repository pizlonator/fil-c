#include <stdio.h>

long lf_basic(unsigned long idx);
long lf_alu(unsigned long idx);

int main() {
    // A LONG-form `#! stack buffer (x, %rsp, %rsp + 32)` on accesses whose
    // base is a computed buffer-address web: the declaration is independent
    // of the access's base register, and the accesses lower to runtime
    // bounds checks against the declared range.
    if (lf_basic(0) != 0x1122334455667788LL) { printf("FAIL lf_basic\n"); return 1; }
    if (lf_alu(0) != 0xfeedface05060708LL) { printf("FAIL lf_alu\n"); return 1; }
    printf("stackbuf longform byaddr att ok\n");
    return 0;
}
