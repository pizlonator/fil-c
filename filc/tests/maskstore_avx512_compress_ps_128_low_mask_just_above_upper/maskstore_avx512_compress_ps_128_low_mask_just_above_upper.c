#include <stdfil.h>

#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 16);

    __m128 v = _mm_setr_ps(0, 1, 2, 3);
    __mmask8 mask = 0x3; /* only low elements active */

    /* popcount=2 -> two elements written at buf + 4, just above upper bound. */
    _mm_mask_compressstoreu_ps(buf + 4, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
