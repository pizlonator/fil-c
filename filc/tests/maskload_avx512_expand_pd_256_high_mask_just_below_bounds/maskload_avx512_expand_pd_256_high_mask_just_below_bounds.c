#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);
    __m256d src = _mm256_set1_pd(-1.0);
    __mmask8 mask = 0xC; /* only high elements active */

    /* For expandload the first active element is read from ptr.
       popcount=2 -> 16 bytes accessed starting at buf - 1, which is OOB. */
    __m256d r = _mm256_mask_expandloadu_pd(src, mask, buf - 1);
    _mm256_storeu_pd(buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
