#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw,avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAB;

    __m256i v = _mm256_set_epi16(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    __mmask16 mask = 0;

    /* Zero mask with an OOB pointer must not access memory. */
    _mm256_mask_cvtepi16_storeu_epi8(buf + 64, mask, v);

    for (int i = 0; i < 16; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAB);

    zprintf("maskstore_avx512_pmov_wb_256 zero mask oob test passed!\n");
    return 0;
}
