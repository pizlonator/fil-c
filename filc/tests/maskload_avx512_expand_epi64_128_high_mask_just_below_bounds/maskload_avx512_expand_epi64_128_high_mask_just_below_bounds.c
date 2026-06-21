#include <stdfil.h>

#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 2 * sizeof(long long));
    __m128i src = _mm_set1_epi64x(-1);
    __mmask8 mask = 0x2;

    /* For expandload the first active element is read from ptr.
       popcount=1 -> one element accessed starting at buf - 1, which is OOB. */
    __m128i r = _mm_mask_expandloadu_epi64(src, mask, buf - 1);
    long long* out = zgc_aligned_alloc(64, 2 * sizeof(long long));
    _mm_storeu_si128((__m128i*)out, r);

    zprintf("Should have failed!\n");
    return 0;
}
