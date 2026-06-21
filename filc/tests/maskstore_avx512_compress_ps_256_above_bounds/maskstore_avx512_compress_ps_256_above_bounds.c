#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 32);
    __m256 v = _mm256_setr_ps(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
    __mmask8 mask = 0xFF;
    _mm256_mask_compressstoreu_ps(buf + 8, mask, v);
    zprintf("Should have failed!\n");
    return 0;
}
