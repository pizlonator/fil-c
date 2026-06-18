#include <stdfil.h>
#include <emmintrin.h>
#include <string.h>

__attribute__((target("sse4.1"))) int main(void)
{
    unsigned char abytes[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    unsigned char bbytes[16];
    memset(bbytes, 1, sizeof(bbytes));

    __m128i a;
    memcpy(&a, abytes, sizeof(a));
    __m128i b;
    memcpy(&b, bbytes, sizeof(b));

    asm volatile("mpsadbw $0, %1, %0" : "+x"(a) : "x"(b));

    unsigned short r[8];
    memcpy(r, &a, sizeof(r));

    ZASSERT(r[0] == 6);
    ZASSERT(r[1] == 10);
    ZASSERT(r[2] == 14);
    ZASSERT(r[3] == 18);
    ZASSERT(r[4] == 22);
    ZASSERT(r[5] == 26);
    ZASSERT(r[6] == 30);
    ZASSERT(r[7] == 34);

    return 0;
}
