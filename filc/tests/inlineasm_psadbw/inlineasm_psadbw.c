#include <stdfil.h>

typedef unsigned char v16uqi __attribute__((vector_size(16)));

int main(void)
{
    /* PSADBW (SSE2): compute sum of absolute differences of packed bytes.
       For the 128-bit XMM form, the 8 low bytes and 8 high bytes are summed
       separately, producing two 16-bit results in bits [15:0] and [79:64];
       all other bits are zeroed. */
    {
        v16uqi a = { 1, 2, 3, 4, 5, 6, 7, 8,
                     1, 1, 1, 1, 1, 1, 1, 1 };
        v16uqi b = { 0, 0, 0, 0, 0, 0, 0, 0,
                     0, 0, 0, 0, 0, 0, 0, 0 };
        asm volatile("psadbw %1, %0" : "+x"(a) : "x"(b));
        unsigned short *p = (unsigned short *)&a;
        ZASSERT(p[0] == 36);   /* 1+2+3+4+5+6+7+8 */
        ZASSERT(p[1] == 0);
        ZASSERT(p[2] == 0);
        ZASSERT(p[3] == 0);
        ZASSERT(p[4] == 8);    /* 1*8 */
        ZASSERT(p[5] == 0);
        ZASSERT(p[6] == 0);
        ZASSERT(p[7] == 0);
    }

    return 0;
}
