#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 4 * sizeof(int));
    for (int i = 0; i < 4; i++)
        buf[i] = 0xABABABAB;
    __m128i src = _mm_set1_epi32(-1);
    __mmask8 mask = 0x03; /* only low elements active */
    __m128i r = _mm_mask_expandloadu_epi32(src, mask, buf + 2);
    int* out = zgc_aligned_alloc(64, 4 * sizeof(int));
    _mm_storeu_si128((__m128i*)out, r);
    ZASSERT(out[0] == 0xABABABAB);
    ZASSERT(out[1] == 0xABABABAB);
    ZASSERT(out[2] == -1);
    ZASSERT(out[3] == -1);
    zprintf("expand epi32 128 low mask near upper test passed!\n");
    return 0;
}
