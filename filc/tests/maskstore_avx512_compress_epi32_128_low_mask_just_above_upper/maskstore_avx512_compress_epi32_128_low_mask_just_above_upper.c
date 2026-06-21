#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);

    __m128i v = _mm_setr_epi32(0, 1, 2, 3);
    __mmask8 mask = 0x1; /* only low elements active */

    /* popcount=1 -> one element written at buf + 16, just above upper bound. */
    _mm_mask_compressstoreu_epi32(buf + 16, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
