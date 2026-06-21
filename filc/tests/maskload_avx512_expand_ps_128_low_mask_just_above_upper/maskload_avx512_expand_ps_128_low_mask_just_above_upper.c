#include <stdfil.h>

#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 16);
    __m128 src = _mm_set1_ps(-1.0f);
    __mmask8 mask = 0x3; /* only low elements active */

    /* popcount=2 -> two elements read at buf + 4, just above upper bound. */
    __m128 r = _mm_mask_expandloadu_ps(src, mask, buf + 4);
    _mm_storeu_ps(buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
