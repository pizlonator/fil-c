#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);
    __m256i src = _mm256_set1_epi32(-1);
    __mmask8 mask = 0xF0; /* only high elements active */

    /* For expandload the first active element is read from ptr.
       popcount=4 -> 16 bytes accessed starting at buf - 1, which is OOB. */
    __m256i r = _mm256_mask_expandloadu_epi32(src, mask, buf - 1);
    _mm256_storeu_si256((__m256i*)buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
