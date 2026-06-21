#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);

    __m512d v = _mm512_setr_pd(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0xF0; /* only high elements active */

    /* For compressstore the first active element is written at ptr.
       Access starts at buf + -4, which is OOB. */
    _mm512_mask_compressstoreu_pd(buf + -4, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
