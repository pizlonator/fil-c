#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 32; i++) buf[i] = 0xAB;

    __m512i v = _mm512_set_epi16(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    __mmask32 mask = 0;

    /* Zero mask with an OOB pointer must not access memory. */
    _mm512_mask_cvtepi16_storeu_epi8(buf + 64, mask, v);

    for (int i = 0; i < 32; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAB);

    zprintf("maskstore_avx512_pmov_wb_512 zero mask oob test passed!\n");
    return 0;
}
