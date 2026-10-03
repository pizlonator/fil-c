#include <stdio.h>
#include <stdlib.h>

void swap_loop2(unsigned char* inp, unsigned char* out, long len);

/* Mirrors the aesni CBC-encrypt cases: len==0 touches nothing, len>=4 takes
   the main loop (len%4==0 exits straight to done), and a nonzero remainder
   goes through the xchg tail (copied bytes transform in place, zero padding
   transforms to 0xff). */
static int check(unsigned char* inp, unsigned char* out, long len)
{
    for (long i = 0; i < len; i++) {
        unsigned char want = (unsigned char)(inp[i] ^ 0xff);
        if (out[i] != want) {
            printf(
                "FAIL: out[%ld]=%u want %u (len=%ld)\n",
                i,
                out[i],
                want,
                len);
            return 1;
        }
    }
    if (len % 4 != 0) {
        long blk = (len + 3) & ~3L;
        for (long i = len; i < blk; i++) {
            if (out[i] != 0xff) {
                printf(
                    "FAIL: pad[%ld]=%u want 255 (len=%ld)\n",
                    i,
                    out[i],
                    len);
                return 1;
            }
        }
    }
    return 0;
}

static int run_case(long len)
{
    unsigned char* inp = malloc(16);
    unsigned char* out = malloc(16);
    if (!inp || !out)
        return 1;
    for (long i = 0; i < 16; i++)
        inp[i] = (unsigned char)(i * 17 + 3);
    for (long i = 0; i < 16; i++)
        out[i] = 0xAA;
    swap_loop2(inp, out, len);
    int ret = check(inp, out, len);
    /* bytes past the padded block must be untouched */
    if (!ret) {
        long blk = (len + 3) & ~3L;
        for (long i = blk; i < 16; i++) {
            if (out[i] != 0xAA) {
                printf("FAIL: out[%ld] touched (len=%ld)\n", i, len);
                ret = 1;
            }
        }
    }
    free(inp);
    free(out);
    return ret;
}

int main(void)
{
    /* main loop only, single and multiple blocks */
    if (run_case(4))
        return 1;
    if (run_case(16))
        return 1;
    /* main loop followed by the xchg tail path */
    if (run_case(5))
        return 1;
    if (run_case(9))
        return 1;
    /* tail path only */
    if (run_case(3))
        return 1;
    /* neither path must touch memory */
    if (run_case(0))
        return 1;
    printf("xchg loop att ok\n");
    return 0;
}
