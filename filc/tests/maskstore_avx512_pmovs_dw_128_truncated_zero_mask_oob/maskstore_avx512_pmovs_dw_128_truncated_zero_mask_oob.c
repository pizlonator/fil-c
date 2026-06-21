#include <stdfil.h>

#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 8);
    for (int i = 0; i < 8; i++) buf[i] = 0xAB;

    __m128i v = _mm_setr_epi32(0, 1, 2, 3);
    __mmask8 mask = 0xF0;   /* high bits set, low 4 bits zero after truncation */

    /* Truncated zero mask with an OOB pointer must not access memory. */
    _mm_mask_cvtsepi32_storeu_epi16(buf + 64, mask, v);

    for (int i = 0; i < 8; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAB);

    zprintf("maskstore_avx512_pmovs_dw_128_truncated_zero_mask_oob test passed!\n");
    return 0;
}
