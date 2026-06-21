#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 16);
    float* out = zgc_aligned_alloc(64, 16);
    __m128 src = _mm_set1_ps(-1.0f);
    __mmask8 mask = 0x0F;
    __m128 r = _mm_mask_expandloadu_ps(src, mask, buf + 4);
    _mm_storeu_ps(out, r);
    zprintf("Should have failed!\n");
    return 0;
}
