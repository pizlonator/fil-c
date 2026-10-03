/* Reduced Camellia SBOX-prefetch + rep movsb main: verify the countdown ran
   exactly 32 iterations (the checksum covers exactly the 128-stride table
   qwords), the annotated calls still work with the counter web live, and the
   rep copy landed. */
#include <stdio.h>
#include <string.h>

long warm_copy(void *dst, void *src, long n);

/* The .s table holds sbox[j] = j*0x9E3779B97F4A7C15 + 0x123456789ABCDEF. */
static unsigned long T(unsigned long j)
{
    return j * 0x9E3779B97F4A7C15UL + 0x123456789ABCDEFUL;
}

int main()
{
    unsigned char src[64], dst[64];
    for (int i = 0; i < 64; i++) { src[i] = (unsigned char)(i * 3 + 5); dst[i] = 0; }

    /* The loop loads table qwords at byte offsets 0,32,64,96 + 128*i and
       XORs them into the result. */
    unsigned long warm = 0;
    for (unsigned long i = 0; i < 32; i++) {
        warm ^= T(16 * i) ^ T(16 * i + 4) ^ T(16 * i + 8) ^ T(16 * i + 12);
    }

    unsigned long expect = warm + 8 + 9;  /* accent(1) = 8, accent(2) = 9 */
    unsigned long first8;
    memcpy(&first8, src, 8);
    expect += first8;                     /* read-back of the copied bytes */

    unsigned long got = warm_copy(dst, src, 16);
    if (got != expect) {
        printf("BAD checksum %lu != %lu\n", (unsigned long)got, (unsigned long)expect);
        return 1;
    }
    if (memcmp(dst, src, 16) != 0) {
        printf("BAD copy\n");
        return 1;
    }
    printf("rep counter att ok\n");
    return 0;
}
