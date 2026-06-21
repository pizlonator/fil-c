#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

    /* Full mask */
    __mmask16 full = 0xFFFF;
    _mm512_mask_cvtepi32_storeu_epi16(buf, full, v);
    for (int i = 0; i < 16; i++)
        ZASSERT(((unsigned short*)buf)[i] == (unsigned short)i);

    /* Partial mask (odd elements) */
    for (int i = 0; i < 64; i++) buf[i] = 0;
    __mmask16 partial = 0xAAAA;
    _mm512_mask_cvtepi32_storeu_epi16(buf, partial, v);
    for (int i = 0; i < 16; i++) {
            if (i % 2 == 1)
                ZASSERT(((unsigned short*)buf)[i] == (unsigned short)i);
            else
                ZASSERT(((unsigned short*)buf)[i] == 0);
        }

    /* Zero mask - no access */
    for (int i = 0; i < 64; i++) buf[i] = 42;
    __mmask16 zero = 0;
    _mm512_mask_cvtepi32_storeu_epi16(buf, zero, v);
    for (int i = 0; i < 64; i++)
        ZASSERT(((unsigned char*)buf)[i] == 42);

    zprintf("pmov_dw_512 basic test passed!\n");
    return 0;
}
