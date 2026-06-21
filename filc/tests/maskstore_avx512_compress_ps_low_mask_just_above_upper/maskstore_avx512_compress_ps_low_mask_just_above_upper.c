#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);

    __m512 v = _mm512_setr_ps(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __mmask16 mask = 0x0001; /* only element 0 active */

    /* popcount=1 -> one element written at buf + 16, just above upper bound. */
    _mm512_mask_compressstoreu_ps(buf + 16, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
