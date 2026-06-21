#include <stdfil.h>

#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 2 * sizeof(double));

    __m128d v = _mm_setr_pd(0, 1);
    __mmask8 mask = 0x2;

    /* For compressstore the first active element is written at ptr.
       popcount=1 -> one element accessed starting at buf - 1, which is OOB. */
    _mm_mask_compressstoreu_pd(buf - 1, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
