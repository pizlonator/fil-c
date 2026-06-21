#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m256i v = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);

    /* Full mask */
    __mmask8 full = 0xFF;
    _mm256_mask_cvtusepi32_storeu_epi16(buf, full, v);
    for (int i = 0; i < 8; i++)
        ZASSERT(((unsigned short*)buf)[i] == (unsigned short)i);

    /* Partial mask (odd elements) */
    for (int i = 0; i < 64; i++) buf[i] = 0;
    __mmask8 partial = 0xAA;
    _mm256_mask_cvtusepi32_storeu_epi16(buf, partial, v);
    for (int i = 0; i < 8; i++) {
            if (i % 2 == 1)
                ZASSERT(((unsigned short*)buf)[i] == (unsigned short)i);
            else
                ZASSERT(((unsigned short*)buf)[i] == 0);
        }

    /* Zero mask - no access */
    for (int i = 0; i < 64; i++) buf[i] = 42;
    __mmask8 zero = 0;
    _mm256_mask_cvtusepi32_storeu_epi16(buf, zero, v);
    for (int i = 0; i < 64; i++)
        ZASSERT(((unsigned char*)buf)[i] == 42);

    zprintf("pmov_dw_256 basic test passed!\n");
    return 0;
}
