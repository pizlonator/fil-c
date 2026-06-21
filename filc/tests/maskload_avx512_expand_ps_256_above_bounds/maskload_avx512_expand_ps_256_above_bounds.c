#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 32);
    float* out = zgc_aligned_alloc(64, 32);
    __m256 src = _mm256_set1_ps(-1.0f);
    __mmask8 mask = 0xFF;
    __m256 r = _mm256_mask_expandloadu_ps(src, mask, buf + 8);
    _mm256_storeu_ps(out, r);
    zprintf("Should have failed!\n");
    return 0;
}
