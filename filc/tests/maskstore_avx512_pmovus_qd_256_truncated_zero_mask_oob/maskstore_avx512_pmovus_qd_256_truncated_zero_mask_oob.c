#include <stdfil.h>

#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAB;

    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);
    __mmask8 mask = 0xF0;   /* high bits set, low 4 bits zero after truncation */

    /* Truncated zero mask with an OOB pointer must not access memory. */
    _mm256_mask_cvtusepi64_storeu_epi32(buf + 64, mask, v);

    for (int i = 0; i < 16; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAB);

    zprintf("maskstore_avx512_pmovus_qd_256_truncated_zero_mask_oob test passed!\n");
    return 0;
}
