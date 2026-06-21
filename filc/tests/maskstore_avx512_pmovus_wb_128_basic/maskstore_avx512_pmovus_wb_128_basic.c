#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw,avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m128i v = _mm_set_epi16(7, 6, 5, 4, 3, 2, 1, 0);

    /* Full mask */
    __mmask8 full = 0xFF;
    _mm_mask_cvtusepi16_storeu_epi8(buf, full, v);
    for (int i = 0; i < 8; i++)
        ZASSERT(((unsigned char*)buf)[i] == (unsigned char)i);

    /* Partial mask (odd elements) */
    for (int i = 0; i < 64; i++) buf[i] = 0;
    __mmask8 partial = 0xAA;
    _mm_mask_cvtusepi16_storeu_epi8(buf, partial, v);
    for (int i = 0; i < 8; i++) {
            if (i % 2 == 1)
                ZASSERT(((unsigned char*)buf)[i] == (unsigned char)i);
            else
                ZASSERT(((unsigned char*)buf)[i] == 0);
        }

    /* Zero mask - no access */
    for (int i = 0; i < 64; i++) buf[i] = 42;
    __mmask8 zero = 0;
    _mm_mask_cvtusepi16_storeu_epi8(buf, zero, v);
    for (int i = 0; i < 64; i++)
        ZASSERT(((unsigned char*)buf)[i] == 42);

    zprintf("pmov_wb_128 basic test passed!\n");
    return 0;
}
