#include <stdfil.h>

#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 4);
    for (int i = 0; i < 4; i++) buf[i] = 0xAB;

    __m128i v = _mm_set_epi64x(1, 0);
    __mmask8 mask = 0xF0;   /* high bits set, low 2 bits zero after truncation */

    /* Truncated zero mask with an OOB pointer must not access memory. */
    _mm_mask_cvtusepi64_storeu_epi16(buf + 64, mask, v);

    for (int i = 0; i < 4; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAB);

    zprintf("maskstore_avx512_pmovus_qw_128_truncated_zero_mask_oob test passed!\n");
    return 0;
}
