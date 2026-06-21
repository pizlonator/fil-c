#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 8 * sizeof(int));
    for (int i = 0; i < 8; i++)
        buf[i] = 0xABABABAB;
    __m256i src = _mm256_set1_epi32(-1);
    __mmask8 mask = 0x0F; /* only low elements active */
    __m256i r = _mm256_mask_expandloadu_epi32(src, mask, buf + 4);
    int* out = zgc_aligned_alloc(64, 8 * sizeof(int));
    _mm256_storeu_si256((__m256i*)out, r);
    ZASSERT(out[0] == 0xABABABAB);
    ZASSERT(out[1] == 0xABABABAB);
    ZASSERT(out[2] == 0xABABABAB);
    ZASSERT(out[3] == 0xABABABAB);
    ZASSERT(out[4] == -1);
    ZASSERT(out[5] == -1);
    ZASSERT(out[6] == -1);
    ZASSERT(out[7] == -1);
    zprintf("expand epi32 256 low mask near upper test passed!\n");
    return 0;
}
