#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);

    __m256 v = _mm256_setr_ps(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
    __mmask8 mask = 0xF0; /* only high elements active */

    /* For compressstore the first active element is written at ptr.
       popcount=4 -> 16 bytes accessed starting at buf - 1, which is OOB. */
    _mm256_mask_compressstoreu_ps(buf - 1, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
