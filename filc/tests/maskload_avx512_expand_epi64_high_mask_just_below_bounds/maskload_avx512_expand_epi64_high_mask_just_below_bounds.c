#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 64);
    __m512i src = _mm512_set1_epi64(-1);
    __mmask8 mask = 0xF0; /* only high elements active */

    /* For expandload the first active element is read from ptr.
       Access starts at buf + -4, which is OOB. */
    __m512i r = _mm512_mask_expandloadu_epi64(src, mask, buf + -4);
    _mm512_storeu_si512((__m512i*)buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
