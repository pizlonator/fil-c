#include <stdfil.h>

#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 8);
    for (int i = 0; i < 8; i++) buf[i] = 0xAB;

    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);
    __mmask8 mask = 0xF0;   /* high bits set, low 4 bits zero after truncation */

    /* Truncated zero mask with an OOB pointer must not access memory. */
    _mm256_mask_cvtsepi64_storeu_epi16(buf + 64, mask, v);

    for (int i = 0; i < 8; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAB);

    zprintf("maskstore_avx512_pmovs_qw_256_truncated_zero_mask_oob test passed!\n");
    return 0;
}
