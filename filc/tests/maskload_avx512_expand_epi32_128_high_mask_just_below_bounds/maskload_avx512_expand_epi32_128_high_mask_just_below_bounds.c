#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);
    __m128i src = _mm_set1_epi32(-1);
    __mmask8 mask = 0xC; /* only high elements active */

    /* For expandload the first active element is read from ptr.
       popcount=2 -> 8 bytes accessed starting at buf - 1, which is OOB. */
    __m128i r = _mm_mask_expandloadu_epi32(src, mask, buf - 1);
    _mm_storeu_si128((__m128i*)buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
