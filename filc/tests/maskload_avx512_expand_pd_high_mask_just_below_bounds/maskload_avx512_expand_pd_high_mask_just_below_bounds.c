#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);
    __m512d src = _mm512_set1_pd(-1.0);
    __mmask8 mask = 0xF0; /* only high elements active */

    /* For expandload the first active element is read from ptr.
       Access starts at buf + -4, which is OOB. */
    __m512d r = _mm512_mask_expandloadu_pd(src, mask, buf + -4);
    _mm512_storeu_pd(buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
