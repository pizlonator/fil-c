#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);

    __m512d v = _mm512_setr_pd(0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0);
    __mmask8 mask = 0xF0; /* only high elements active */

    /* For compressstore the first active element is written at ptr.
       popcount=4 -> 32 bytes accessed starting at buf - 1, which is OOB. */
    _mm512_mask_compressstoreu_pd(buf - 1, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
