#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m128i v = _mm_set_epi64x(1, 0);
    __mmask8 mask = 0x02;

    /* High 1 elements active, starting just below the lower bound. */
    _mm_mask_cvtepi64_storeu_epi32(buf - 4, mask, v);

    ZASSERT(*((unsigned int*)(buf + 0)) == (unsigned int)1);

    zprintf("pmov_qd_128 masked below test passed!\n");
    return 0;
}
