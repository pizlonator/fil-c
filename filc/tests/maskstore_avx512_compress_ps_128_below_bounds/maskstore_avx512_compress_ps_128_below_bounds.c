#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 16);
    __m128 v = _mm_setr_ps(0.0f, 1.0f, 2.0f, 3.0f);
    __mmask8 mask = 0x0F;
    _mm_mask_compressstoreu_ps(buf - 4, mask, v);
    zprintf("Should have failed!\n");
    return 0;
}
