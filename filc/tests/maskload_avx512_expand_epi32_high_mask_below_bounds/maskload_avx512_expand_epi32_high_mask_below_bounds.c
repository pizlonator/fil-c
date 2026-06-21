#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);
    __m512i src = _mm512_set1_epi32(-1);
    __mmask16 mask = 0xFF00; /* only high 8 elements active */

    /* For expandload the first active element is read from ptr.
       popcount=8 -> 32 bytes accessed starting at buf-32, which is OOB. */
    __m512i r = _mm512_mask_expandloadu_epi32(src, mask, buf - 8);
    _mm512_storeu_si512((__m512i*)buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
