#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAB;

    __m256i v = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0;

    /* Zero mask with an OOB pointer must not access memory. */
    _mm256_mask_cvtepi32_storeu_epi16(buf + 64, mask, v);

    for (int i = 0; i < 16; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAB);

    zprintf("maskstore_avx512_pmov_dw_256 zero mask oob test passed!\n");
    return 0;
}
