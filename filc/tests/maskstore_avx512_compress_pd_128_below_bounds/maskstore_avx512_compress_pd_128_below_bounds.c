#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 16);
    __m128d v = _mm_setr_pd(0.0, 1.0);
    __mmask8 mask = 0x03;
    _mm_mask_compressstoreu_pd(buf - 2, mask, v);
    zprintf("Should have failed!\n");
    return 0;
}
