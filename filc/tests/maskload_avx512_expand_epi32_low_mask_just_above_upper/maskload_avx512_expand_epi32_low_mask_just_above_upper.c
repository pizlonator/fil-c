#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);
    __m512i src = _mm512_set1_epi32(-1);
    __mmask16 mask = 0x0001; /* only element 0 active */

    /* popcount=1 -> 4 bytes read at buf+64..buf+67, which is just above upper bound. */
    __m512i r = _mm512_mask_expandloadu_epi32(src, mask, buf + 16);
    _mm512_storeu_si512((__m512i*)buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
