#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);
    __m512d src = _mm512_set1_pd(-1.0);
    __mmask8 mask = 0x1; /* only low elements active */

    /* popcount=1 -> one element read at buf + 8, just above upper bound. */
    __m512d r = _mm512_mask_expandloadu_pd(src, mask, buf + 8);
    _mm512_storeu_pd(buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
