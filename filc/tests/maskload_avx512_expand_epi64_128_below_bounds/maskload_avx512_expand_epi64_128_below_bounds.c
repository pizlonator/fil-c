#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 16);
    long long* out = zgc_aligned_alloc(64, 16);
    __m128i src = _mm_set1_epi64x(-1);
    __mmask8 mask = 0x03;
    __m128i r = _mm_mask_expandloadu_epi64(src, mask, buf - 2);
    _mm_storeu_si128((__m128i*)out, r);
    zprintf("Should have failed!\n");
    return 0;
}
