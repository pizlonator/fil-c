#include <stdfil.h>

#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 2 * sizeof(double));
    __m128d src = _mm_set1_pd(-1.0);
    __mmask8 mask = 0x1;

    /* For expandload the first active element is read from ptr.
       popcount=1 -> one element accessed starting at buf + 2, which is OOB. */
    __m128d r = _mm_mask_expandloadu_pd(src, mask, buf + 2);
    double* out = zgc_aligned_alloc(64, 2 * sizeof(double));
    _mm_storeu_pd(out, r);

    zprintf("Should have failed!\n");
    return 0;
}
