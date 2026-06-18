#include <stdfil.h>
#include <immintrin.h>
#include <string.h>

__attribute__((target("avx2"))) int main(void)
{
    unsigned char abytes[32];
    unsigned char bbytes[32];
    for (int i = 0; i < 32; ++i) {
        abytes[i] = (unsigned char)(i + 1);
        bbytes[i] = 1;
    }

    __m256i a;
    memcpy(&a, abytes, sizeof(a));
    __m256i b;
    memcpy(&b, bbytes, sizeof(b));
    __m256i result;

    asm volatile("vmpsadbw $0, %2, %1, %0" : "=x"(result) : "x"(a), "x"(b));

    unsigned short r[16];
    memcpy(r, &result, sizeof(r));

    for (int i = 0; i < 8; ++i)
        ZASSERT(r[i] == (unsigned short)(6 + i * 4));
    for (int i = 0; i < 8; ++i)
        ZASSERT(r[8 + i] == (unsigned short)(70 + i * 4));

    return 0;
}
