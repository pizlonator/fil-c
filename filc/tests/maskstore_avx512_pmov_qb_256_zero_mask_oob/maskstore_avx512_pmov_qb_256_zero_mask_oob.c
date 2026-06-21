#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 4);
    for (int i = 0; i < 4; i++) buf[i] = 0xAB;

    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);
    __mmask8 mask = 0;

    /* Zero mask with an OOB pointer must not access memory. */
    _mm256_mask_cvtepi64_storeu_epi8(buf + 64, mask, v);

    for (int i = 0; i < 4; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAB);

    zprintf("maskstore_avx512_pmov_qb_256 zero mask oob test passed!\n");
    return 0;
}
