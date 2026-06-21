#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 8; i++) buf[i] = i;
    long long* out = zgc_aligned_alloc(64, 64);
    __m512i src = _mm512_set1_epi64(-1);
    __m512i r;
    __mmask8 mask = 0xF0;
    r = _mm512_mask_expandloadu_epi64(src, mask, buf);
    _mm512_storeu_si512((__m512i*)out, r);
    ZASSERT(out[0] == -1);
    ZASSERT(out[1] == -1);
    ZASSERT(out[2] == -1);
    ZASSERT(out[3] == -1);
    ZASSERT(out[4] == 0);
    ZASSERT(out[5] == 1);
    ZASSERT(out[6] == 2);
    ZASSERT(out[7] == 3);

    zprintf("expandload masked below test passed!\n");
    return 0;
}
