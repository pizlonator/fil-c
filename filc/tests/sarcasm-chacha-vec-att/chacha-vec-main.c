/* Self-contained vector+scalar mix check (fully self-contained): exercises the
 * synthetic tail in chacha-vec.s — which stages its block with 8-byte
 * GPR stores plus one aligned 16-byte vector store and consumes it as
 * 8-byte windows — for every length 0..64. Each length is compared
 * against a plain-C ref (src ^ blk), with the overstep check folded in:
 * dst starts filled with a sentinel and bytes past the length must keep
 * it (the tail must not overrun).
 */
#include <stdio.h>
#include <string.h>

void chacha_vec_xor(void *dst, void *src, void *blk, unsigned long n);

int main() {
    unsigned char src[64], blk[64], dst[64], ref[64];
    for (int i = 0; i < 64; i++) {
        src[i] = (unsigned char)(i * 5 + 1);
        blk[i] = (unsigned char)(i * 7 + 3);
    }
    for (int n = 0; n <= 64; n++) {
        memset(dst, 0xA5, sizeof dst);
        memset(ref, 0xA5, sizeof ref);
        for (int i = 0; i < n; i++)
            ref[i] = (unsigned char)(src[i] ^ blk[i]);
        chacha_vec_xor(dst, src, blk, (unsigned long)n);
        if (memcmp(dst, ref, sizeof dst) != 0) {
            printf("chacha vec BAD n=%d\n", n);
            for (int i = 0; i < 64; i++)
                if (dst[i] != ref[i]) {
                    printf("  at=%d got=%02x want=%02x\n",
                        i, dst[i], ref[i]);
                    break;
                }
            return 1;
        }
    }
    printf("chacha vectors ok\n");
    return 0;
}
