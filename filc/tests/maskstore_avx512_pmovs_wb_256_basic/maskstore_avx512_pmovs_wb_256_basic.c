#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw,avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m256i v = _mm256_set_epi16(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);

    /* Full mask */
    __mmask16 full = 0xFFFF;
    _mm256_mask_cvtsepi16_storeu_epi8(buf, full, v);
    for (int i = 0; i < 16; i++)
        ZASSERT(((unsigned char*)buf)[i] == (unsigned char)i);

    /* Partial mask (odd elements) */
    for (int i = 0; i < 64; i++) buf[i] = 0;
    __mmask16 partial = 0xAAAA;
    _mm256_mask_cvtsepi16_storeu_epi8(buf, partial, v);
    for (int i = 0; i < 16; i++) {
            if (i % 2 == 1)
                ZASSERT(((unsigned char*)buf)[i] == (unsigned char)i);
            else
                ZASSERT(((unsigned char*)buf)[i] == 0);
        }

    /* Zero mask - no access */
    for (int i = 0; i < 64; i++) buf[i] = 42;
    __mmask16 zero = 0;
    _mm256_mask_cvtsepi16_storeu_epi8(buf, zero, v);
    for (int i = 0; i < 64; i++)
        ZASSERT(((unsigned char*)buf)[i] == 42);

    zprintf("pmov_wb_256 basic test passed!\n");
    return 0;
}
