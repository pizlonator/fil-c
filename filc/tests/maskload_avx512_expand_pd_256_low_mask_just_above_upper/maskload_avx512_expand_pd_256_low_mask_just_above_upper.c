#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);
    __m256d src = _mm256_set1_pd(-1.0);
    __mmask8 mask = 0x1; /* only low elements active */

    /* popcount=1 -> one element read at buf + 8, just above upper bound. */
    __m256d r = _mm256_mask_expandloadu_pd(src, mask, buf + 8);
    _mm256_storeu_pd(buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
