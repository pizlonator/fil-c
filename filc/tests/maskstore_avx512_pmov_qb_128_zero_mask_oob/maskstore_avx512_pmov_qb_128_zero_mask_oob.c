#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 2);
    for (int i = 0; i < 2; i++) buf[i] = 0xAB;

    __m128i v = _mm_set_epi64x(1, 0);
    __mmask8 mask = 0;

    /* Zero mask with an OOB pointer must not access memory. */
    _mm_mask_cvtepi64_storeu_epi8(buf + 64, mask, v);

    for (int i = 0; i < 2; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAB);

    zprintf("maskstore_avx512_pmov_qb_128 zero mask oob test passed!\n");
    return 0;
}
