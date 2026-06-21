#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 16);
    double* out = zgc_aligned_alloc(64, 16);
    __m128d src = _mm_set1_pd(-1.0);
    __mmask8 mask = 0x03;
    __m128d r = _mm_mask_expandloadu_pd(src, mask, buf + 2);
    _mm_storeu_pd(out, r);
    zprintf("Should have failed!\n");
    return 0;
}
