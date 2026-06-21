#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 64);
    __m256i src = _mm256_set1_epi64x(-1);
    __mmask8 mask = 0x1; /* only low elements active */

    /* popcount=1 -> one element read at buf + 8, just above upper bound. */
    __m256i r = _mm256_mask_expandloadu_epi64(src, mask, buf + 8);
    _mm256_storeu_si256((__m256i*)buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
