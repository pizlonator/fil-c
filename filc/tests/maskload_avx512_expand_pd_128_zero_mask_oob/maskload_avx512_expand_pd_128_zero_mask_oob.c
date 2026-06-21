#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 2 * sizeof(double));
    __m128d src = _mm_set1_pd(-1.0);
    __mmask8 mask = 0x00;
    __m128d r = _mm_mask_expandloadu_pd(src, mask, buf + 64);
    double* out = zgc_aligned_alloc(64, 2 * sizeof(double));
    _mm_storeu_pd(out, r);
    for (int i = 0; i < 2; i++)
        ZASSERT(out[i] == -1.0);
    zprintf("expand pd 128 zero mask oob test passed!\n");
    return 0;
}
