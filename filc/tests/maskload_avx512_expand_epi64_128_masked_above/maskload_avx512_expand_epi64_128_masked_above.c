#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 2; i++) buf[i] = i;
    long long* out = zgc_aligned_alloc(64, 16);
    __m128i src = _mm_set1_epi64x(-1);
    __m128i r;
    __mmask8 mask = 0x01;
    r = _mm_mask_expandloadu_epi64(src, mask, buf + 1);
    _mm_storeu_si128((__m128i*)out, r);
    ZASSERT(out[0] == 1);
    ZASSERT(out[1] == -1);

    zprintf("expandload masked above test passed!\n");
    return 0;
}
