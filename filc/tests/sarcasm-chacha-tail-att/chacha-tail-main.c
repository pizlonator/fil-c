#include <stdio.h>
#include <string.h>

void chacha_tail_ecx(void *dst, void *src, void *blk, size_t n);
void chacha_tail_edx(void *dst, void *src, void *blk, size_t n);
long chacha_tail_upper32(long sentinel, void *blk);

int main() {
    char src[64], blk[64], dst[64], ref[64];
    for (int i = 0; i < 64; i++) {
        src[i] = (char)(i * 5 + 1);
        blk[i] = (char)(i * 7 + 3);
    }
    for (int n = 0; n <= 64; n++) {
        memset(dst, 0, sizeof dst);
        memset(ref, 0, sizeof ref);
        for (int i = 0; i < n; i++)
            ref[i] = (char)(src[i] ^ blk[i]);
        chacha_tail_ecx(dst, src, blk, (size_t)n);
        if (memcmp(dst, ref, sizeof dst) != 0) {
            printf("ecx BAD n=%d\n", n);
            return 1;
        }
        memset(dst, 0, sizeof dst);
        chacha_tail_edx(dst, src, blk, (size_t)n);
        if (memcmp(dst, ref, sizeof dst) != 0) {
            printf("edx BAD n=%d\n", n);
            return 1;
        }
    }
    printf("chacha tail windows ok\n");
    // Upper-32 clobber proof: sentinel with a nonzero upper half live in
    // %rcx/%rdx across the tail-shaped scratch, plus four live copies.
    // scratch results are zero-extended ((w>>8) each), sentinels intact.
    {
        char blk[64];
        for (int i = 0; i < 64; i++)
            blk[i] = (char)(i * 7 + 3);
        long S = (long)0x9E3779B97F4A7C15L;
        unsigned long w0, w1;
        memcpy(&w0, blk, 4);
        memcpy(&w1, blk + 4, 4);
        unsigned long expect = (w0 >> 8) + (w1 >> 8) + 4 * (unsigned long)S;
        long got = chacha_tail_upper32(S, blk);
        if ((unsigned long)got != expect) {
            printf("upper32 BAD got=0x%lx expect=0x%lx\n",
                (unsigned long)got, expect);
            return 1;
        }
    }
    printf("chacha tail upper32 ok\n");
    return 0;
}
