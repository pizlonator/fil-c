#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 8; i++)
        buf[i] = -1;
    __m512i src = _mm512_set1_epi64(-1);

    /* Zero mask with an OOB pointer must not access memory. */
    __mmask8 mask = 0;
    __m512i r = _mm512_mask_expandloadu_epi64(src, mask, buf + 8);
    long long* out = zgc_aligned_alloc(64, 64);
    _mm512_storeu_si512((__m512i*)out, r);

    for (int i = 0; i < 8; i++)
        ZASSERT(out[i] == -1);

    zprintf("expand epi64 zero mask oob test passed!\n");
    return 0;
}
