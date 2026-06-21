#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m256i v = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0xF0;

    /* High 4 elements active, starting just below the lower bound. */
    _mm256_mask_cvtepi32_storeu_epi16(buf - 8, mask, v);

    ZASSERT(*((unsigned short*)(buf + 0)) == (unsigned short)4);
    ZASSERT(*((unsigned short*)(buf + 2)) == (unsigned short)5);
    ZASSERT(*((unsigned short*)(buf + 4)) == (unsigned short)6);
    ZASSERT(*((unsigned short*)(buf + 6)) == (unsigned short)7);

    zprintf("pmov_dw_256 masked below test passed!\n");
    return 0;
}
