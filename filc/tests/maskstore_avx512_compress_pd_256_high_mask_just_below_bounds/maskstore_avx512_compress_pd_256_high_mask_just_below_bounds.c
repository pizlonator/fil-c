#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);

    __m256d v = _mm256_setr_pd(0.0, 1.0, 2.0, 3.0);
    __mmask8 mask = 0xC; /* only high elements active */

    /* For compressstore the first active element is written at ptr.
       popcount=2 -> 16 bytes accessed starting at buf - 1, which is OOB. */
    _mm256_mask_compressstoreu_pd(buf - 1, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
