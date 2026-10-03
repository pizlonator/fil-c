#include <stdio.h>
#include <string.h>

void if_ext(void *out, unsigned long n);
long if_idx(unsigned long i);

int main(void) {
    /* rep movsb out of the inner-frame buffer: 15..1 bytes, plus the empty copy */
    for (unsigned long n = 1; n <= 15; n++) {
        unsigned char out[32];
        memset(out, 0xA5, sizeof out);
        if_ext(out, n);
        for (unsigned long i = 0; i < 16 - n; i++)
            if (out[i] != (unsigned char)i) {
                printf("innerframe data n=%lu at=%lu got=%02x\n", n, i, out[i]);
                return 1;
            }
        for (unsigned long i = 16 - n; i < sizeof out; i++)
            if (out[i] != 0xA5) {
                printf("innerframe overrun n=%lu at=%lu\n", n, i);
                return 1;
            }
    }
    /* indexed load in the inner frame, bounds-checked */
    if (if_idx(0) != 0x4142434445464748LL) {
        printf("innerframe idx0 bad\n");
        return 1;
    }
    if (if_idx(8) != 0) {
        printf("innerframe idx8 bad\n");
        return 1;
    }
    printf("innerframe buffer ok\n");
    return 0;
}
