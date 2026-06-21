#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    long long* buf = (long long*)zgc_aligned_alloc(64, 2 * sizeof(long long));
    for (int i = 0; i < 2; i++)
        buf[i] = (long long)i;
    long long* out = (long long*)zgc_aligned_alloc(64, 2 * sizeof(long long));
    for (int i = 0; i < 2; i++)
        out[i] = (long long)-1;
    __m128i src = _mm_set1_epi64x(-1);
    __mmask8 mask = 0xF0;
    __m128i r = _mm_mask_expandloadu_epi64(src, mask, buf + 16);
    _mm_storeu_si128((__m128i*)out, r);
    for (int i = 0; i < 2; i++)
        ZASSERT(out[i] == (long long)-1);
    zprintf("expand epi64 128 truncated zero mask oob test passed!\n");
    return 0;
}
