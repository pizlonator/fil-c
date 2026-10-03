#include <stdio.h>

int nz_basic(unsigned long idx);
int nz_alias(unsigned long idx);

int main() {
    // idx is the byte offset from %rsp; the buffer starts at 16.
    if (nz_basic(20) != 0x11223344) { printf("nz basic 20 BAD\n"); return 1; }
    if (nz_basic(24) != 0x55667788) { printf("nz basic 24 BAD\n"); return 1; }
    // The alias spelling: idx is relative to the alias at %rsp + 16.
    if (nz_alias(24) != 0x99aabbcc) { printf("nz alias 24 BAD\n"); return 1; }
    printf("nonzero base ok\n");
    return 0;
}
