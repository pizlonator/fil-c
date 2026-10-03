#include <stdio.h>

extern long idxmask(unsigned long idx);

int main(void)
{
    // Every index that keeps every access (the -8 read through the
    // 64-byte masked moves) inside the 128-byte buffer.
    for (unsigned long idx = 8; idx <= 64; idx += 8) {
        if (idxmask(idx) != 1) {
            printf("FAIL idxmask %lu\n", idx);
            return 1;
        }
    }
    printf("stackbuf idx masked att ok\n");
    return 0;
}
