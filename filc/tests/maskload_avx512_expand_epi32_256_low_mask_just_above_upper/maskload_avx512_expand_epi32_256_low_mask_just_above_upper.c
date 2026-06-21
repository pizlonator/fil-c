#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);
    __m256i src = _mm256_set1_epi32(-1);
    __mmask8 mask = 0x1; /* only low elements active */

    /* popcount=1 -> one element read at buf + 16, just above upper bound. */
    __m256i r = _mm256_mask_expandloadu_epi32(src, mask, buf + 16);
    _mm256_storeu_si256((__m256i*)buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
