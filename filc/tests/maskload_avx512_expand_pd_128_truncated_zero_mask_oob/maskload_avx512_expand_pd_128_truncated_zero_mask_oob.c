#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    double* buf = (double*)zgc_aligned_alloc(64, 2 * sizeof(double));
    for (int i = 0; i < 2; i++)
        buf[i] = (double)i;
    double* out = (double*)zgc_aligned_alloc(64, 2 * sizeof(double));
    for (int i = 0; i < 2; i++)
        out[i] = (double)-1;
    __m128d src = _mm_set1_pd(-1.0);
    __mmask8 mask = 0xF0;
    __m128d r = _mm_mask_expandloadu_pd(src, mask, buf + 16);
    _mm_storeu_pd(out, r);
    for (int i = 0; i < 2; i++)
        ZASSERT(out[i] == (double)-1);
    zprintf("expand pd 128 truncated zero mask oob test passed!\n");
    return 0;
}
