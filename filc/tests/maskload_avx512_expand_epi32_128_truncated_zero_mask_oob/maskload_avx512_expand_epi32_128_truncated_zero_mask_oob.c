#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    int* buf = (int*)zgc_aligned_alloc(64, 4 * sizeof(int));
    for (int i = 0; i < 4; i++)
        buf[i] = (int)i;
    int* out = (int*)zgc_aligned_alloc(64, 4 * sizeof(int));
    for (int i = 0; i < 4; i++)
        out[i] = (int)-1;
    __m128i src = _mm_set1_epi32(-1);
    __mmask8 mask = 0xF0;
    __m128i r = _mm_mask_expandloadu_epi32(src, mask, buf + 16);
    _mm_storeu_si128((__m128i*)out, r);
    for (int i = 0; i < 4; i++)
        ZASSERT(out[i] == (int)-1);
    zprintf("expand epi32 128 truncated zero mask oob test passed!\n");
    return 0;
}
