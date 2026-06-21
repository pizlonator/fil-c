#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 4 * sizeof(int));
    __m128i src = _mm_set1_epi32(-1);
    __mmask8 mask = 0x00;
    __m128i r = _mm_mask_expandloadu_epi32(src, mask, buf + 64);
    int* out = zgc_aligned_alloc(64, 4 * sizeof(int));
    _mm_storeu_si128((__m128i*)out, r);
    for (int i = 0; i < 4; i++)
        ZASSERT(out[i] == -1);
    zprintf("expand epi32 128 zero mask oob test passed!\n");
    return 0;
}
