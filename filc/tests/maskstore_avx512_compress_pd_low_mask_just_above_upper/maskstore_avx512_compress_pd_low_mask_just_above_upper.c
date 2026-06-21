#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);

    __m512d v = _mm512_setr_pd(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0x01; /* only element 0 active */

    /* popcount=1 -> one element written at buf + 8, just above upper bound. */
    _mm512_mask_compressstoreu_pd(buf + 8, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
