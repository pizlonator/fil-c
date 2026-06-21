#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 8 * sizeof(int));
    __m256i src = _mm256_set1_epi32(-1);
    __mmask8 mask = 0x00;
    __m256i r = _mm256_mask_expandloadu_epi32(src, mask, buf + 64);
    int* out = zgc_aligned_alloc(64, 8 * sizeof(int));
    _mm256_storeu_si256((__m256i*)out, r);
    for (int i = 0; i < 8; i++)
        ZASSERT(out[i] == -1);
    zprintf("expand epi32 256 zero mask oob test passed!\n");
    return 0;
}
