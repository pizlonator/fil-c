#include <stdio.h>
#include <string.h>

extern long repesc(void *src, void *out);

void seed8(long *p)
{
    p[0] = 0x1111;
    p[1] = 0x2222;
}

/* The checksum the asm sequence must produce: the seed area (0x1111 +
   0x2222), three of the eight rep-stosq pattern qwords (3 * 0xC0DE), and the
   first/last of the 100 round-tripped bytes (src[0] + src[99]). */
static long mirror_sum(const unsigned char *src)
{
    return (long)0x1111 + (long)0x2222 + 3L * (long)0xC0DE + src[0] + src[99];
}

int main(void)
{
    unsigned char src[100], out[100];
    for (int i = 0; i < 100; i++) src[i] = (unsigned char)(i + 1);
    long want = mirror_sum(src);    /* 161330 */
    if (want != 161330) {
        printf("FAIL: mirror self-check got %ld, want 161330\n", want);
        return 1;
    }
    for (int i = 0; i < 2; i++) {
        memset(out, 0xAA, sizeof out);
        long r = repesc(src, out);
        if (memcmp(out, src, 100) != 0) {
            printf("FAIL: round trip not byte-exact\n");
            return 1;
        }
        if (r != 161330) {
            printf("FAIL: got %ld, want 161330\n", r);
            return 1;
        }
    }
    printf("frame escape rep att ok\n");
    return 0;
}
