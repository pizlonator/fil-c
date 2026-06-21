#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 32; i++) buf[i] = 0xAB;

    __m512i v = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);
    __mmask8 mask = 0;

    /* Zero mask with an OOB pointer must not access memory. */
    _mm512_mask_cvtepi64_storeu_epi32(buf + 64, mask, v);

    for (int i = 0; i < 32; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAB);

    zprintf("maskstore_avx512_pmov_qd_512 zero mask oob test passed!\n");
    return 0;
}
