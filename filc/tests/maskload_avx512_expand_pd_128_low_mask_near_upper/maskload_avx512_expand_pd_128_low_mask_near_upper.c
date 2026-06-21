#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 2 * sizeof(double));
    for (int i = 0; i < 2; i++)
        buf[i] = 0xABp0;
    __m128d src = _mm_set1_pd(-1.0);
    __mmask8 mask = 0x01; /* only low elements active */
    __m128d r = _mm_mask_expandloadu_pd(src, mask, buf + 1);
    double* out = zgc_aligned_alloc(64, 2 * sizeof(double));
    _mm_storeu_pd(out, r);
    ZASSERT(out[0] == 0xABp0);
    ZASSERT(out[1] == -1.0);
    zprintf("expand pd 128 low mask near upper test passed!\n");
    return 0;
}
