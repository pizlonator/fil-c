#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);

    __m128i v = _mm_setr_epi32(0, 1, 2, 3);
    __mmask8 mask = 0xC; /* only high elements active */

    /* For compressstore the first active element is written at ptr.
       popcount=2 -> 8 bytes accessed starting at buf - 1, which is OOB. */
    _mm_mask_compressstoreu_epi32(buf - 1, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
