#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m512i v = _mm512_set_epi16(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);

    /* Full mask */
    __mmask32 full = 0xFFFFFFFF;
    _mm512_mask_cvtusepi16_storeu_epi8(buf, full, v);
    for (int i = 0; i < 32; i++)
        ZASSERT(((unsigned char*)buf)[i] == (unsigned char)i);

    /* Partial mask (odd elements) */
    for (int i = 0; i < 64; i++) buf[i] = 0;
    __mmask32 partial = 0xAAAAAAAA;
    _mm512_mask_cvtusepi16_storeu_epi8(buf, partial, v);
    for (int i = 0; i < 32; i++) {
            if (i % 2 == 1)
                ZASSERT(((unsigned char*)buf)[i] == (unsigned char)i);
            else
                ZASSERT(((unsigned char*)buf)[i] == 0);
        }

    /* Zero mask - no access */
    for (int i = 0; i < 64; i++) buf[i] = 42;
    __mmask32 zero = 0;
    _mm512_mask_cvtusepi16_storeu_epi8(buf, zero, v);
    for (int i = 0; i < 64; i++)
        ZASSERT(((unsigned char*)buf)[i] == 42);

    zprintf("pmov_wb_512 basic test passed!\n");
    return 0;
}
