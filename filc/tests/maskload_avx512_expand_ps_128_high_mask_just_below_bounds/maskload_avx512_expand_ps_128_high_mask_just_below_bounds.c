#include <stdfil.h>

#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 16);
    __m128 src = _mm_set1_ps(-1.0f);
    __mmask8 mask = 0xC; /* only high elements active */

    /* For expandload the first active element is read from ptr.
       popcount=2 -> 8 bytes accessed starting at buf - 1, which is OOB. */
    __m128 r = _mm_mask_expandloadu_ps(src, mask, buf - 1);
    _mm_storeu_ps(buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
