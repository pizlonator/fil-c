#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 8; i++) buf[i] = i;

    long long* out = zgc_aligned_alloc(64, 64);
    __m512i src = _mm512_set1_epi64(-1);

    __mmask8 full = 0xFF;
    __m512i r = _mm512_mask_expandloadu_epi64(src, full, buf);
    _mm512_storeu_si512((__m512i*)out, r);
    for (int i = 0; i < 8; i++)
        ZASSERT(out[i] == i);

    __mmask8 partial = 0x0F;
    r = _mm512_mask_expandloadu_epi64(src, partial, buf);
    _mm512_storeu_si512((__m512i*)out, r);
    for (int i = 0; i < 4; i++)
        ZASSERT(out[i] == i);
    for (int i = 4; i < 8; i++)
        ZASSERT(out[i] == -1);

    __mmask8 zero = 0;
    r = _mm512_mask_expandloadu_epi64(src, zero, buf);
    _mm512_storeu_si512((__m512i*)out, r);
    for (int i = 0; i < 8; i++)
        ZASSERT(out[i] == -1);

    zprintf("expand epi64 basic test passed!\n");
    return 0;
}
