#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);
    __mmask8 mask = 0x0C;

    /* High 2 elements active, starting just below the lower bound. */
    _mm256_mask_cvtepi64_storeu_epi16(buf - 4, mask, v);

    ZASSERT(*((unsigned short*)(buf + 0)) == (unsigned short)2);
    ZASSERT(*((unsigned short*)(buf + 2)) == (unsigned short)3);

    zprintf("pmov_qw_256 masked below test passed!\n");
    return 0;
}
