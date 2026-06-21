#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 4 * sizeof(double));
    __m256d src = _mm256_set1_pd(-1.0);
    __mmask8 mask = 0x00;
    __m256d r = _mm256_mask_expandloadu_pd(src, mask, buf + 64);
    double* out = zgc_aligned_alloc(64, 4 * sizeof(double));
    _mm256_storeu_pd(out, r);
    for (int i = 0; i < 4; i++)
        ZASSERT(out[i] == -1.0);
    zprintf("expand pd 256 zero mask oob test passed!\n");
    return 0;
}
