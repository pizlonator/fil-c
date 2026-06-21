#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 64);
    __m512i src = _mm512_set1_epi64(-1);
    __mmask8 mask = 0x1; /* only low elements active */

    /* popcount=1 -> one element read at buf + 8, just above upper bound. */
    __m512i r = _mm512_mask_expandloadu_epi64(src, mask, buf + 8);
    _mm512_storeu_si512((__m512i*)buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
