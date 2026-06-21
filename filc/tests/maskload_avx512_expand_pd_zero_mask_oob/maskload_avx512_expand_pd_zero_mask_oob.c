#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 8; i++)
        buf[i] = -1.0;
    __m512d src = _mm512_set1_pd(-1.0);

    /* Zero mask with an OOB pointer must not access memory. */
    __mmask8 mask = 0;
    __m512d r = _mm512_mask_expandloadu_pd(src, mask, buf + 8);
    double* out = zgc_aligned_alloc(64, 64);
    _mm512_storeu_pd(out, r);

    for (int i = 0; i < 8; i++)
        ZASSERT(out[i] == -1.0);

    zprintf("expand pd zero mask oob test passed!\n");
    return 0;
}
