#include <stdio.h>
#include <string.h>

void rep_oob(unsigned char* dst, unsigned long n);

int main() {
    unsigned char out[64];
    memset(out, 0, sizeof out);
    rep_oob(out, 17);   // one byte past the 16-byte buffer
    printf("copied: %d\n", out[0]);
    return 0;
}
