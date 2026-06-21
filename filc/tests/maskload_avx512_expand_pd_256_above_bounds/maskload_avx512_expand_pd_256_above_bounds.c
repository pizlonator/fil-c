#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 32);
    double* out = zgc_aligned_alloc(64, 32);
    __m256d src = _mm256_set1_pd(-1.0);
    __mmask8 mask = 0x0F;
    __m256d r = _mm256_mask_expandloadu_pd(src, mask, buf + 4);
    _mm256_storeu_pd(out, r);
    zprintf("Should have failed!\n");
    return 0;
}
