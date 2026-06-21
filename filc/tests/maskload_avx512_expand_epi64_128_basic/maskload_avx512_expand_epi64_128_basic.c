#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 2; i++) buf[i] = i;
    long long* out = zgc_aligned_alloc(64, 16);
    __m128i src = _mm_set1_epi64x(-1);
    __m128i r;
    __mmask8 mask;
    __mmask8 odd_mask = 0x02;

    /* Partial odd mask: active lanes are odd-numbered positions. */
    mask = odd_mask;
    r = _mm_mask_expandloadu_epi64(src, mask, buf);
    _mm_storeu_si128((__m128i*)out, r);
    ZASSERT(out[1] == 0);
    ZASSERT(out[0] == -1);

    mask = 0x03;
    r = _mm_mask_expandloadu_epi64(src, mask, buf);
    _mm_storeu_si128((__m128i*)out, r);
    for (int i = 0; i < 2; i++)
        ZASSERT(out[i] == i);

    mask = 0;
    r = _mm_mask_expandloadu_epi64(src, mask, buf);
    _mm_storeu_si128((__m128i*)out, r);
    for (int i = 0; i < 2; i++)
        ZASSERT(out[i] == -1);

    zprintf("expandload basic test passed!\n");
    return 0;
}
