#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 4 * sizeof(double));
    for (int i = 0; i < 4; i++)
        buf[i] = 0xABp0;
    __m256d src = _mm256_set1_pd(-1.0);
    __mmask8 mask = 0x03; /* only low elements active */
    __m256d r = _mm256_mask_expandloadu_pd(src, mask, buf + 2);
    double* out = zgc_aligned_alloc(64, 4 * sizeof(double));
    _mm256_storeu_pd(out, r);
    ZASSERT(out[0] == 0xABp0);
    ZASSERT(out[1] == 0xABp0);
    ZASSERT(out[2] == -1.0);
    ZASSERT(out[3] == -1.0);
    zprintf("expand pd 256 low mask near upper test passed!\n");
    return 0;
}
