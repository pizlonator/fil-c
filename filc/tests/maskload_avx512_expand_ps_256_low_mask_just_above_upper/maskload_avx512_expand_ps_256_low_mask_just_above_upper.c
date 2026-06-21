#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);
    __m256 src = _mm256_set1_ps(-1.0f);
    __mmask8 mask = 0x1; /* only low elements active */

    /* popcount=1 -> one element read at buf + 16, just above upper bound. */
    __m256 r = _mm256_mask_expandloadu_ps(src, mask, buf + 16);
    _mm256_storeu_ps(buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
