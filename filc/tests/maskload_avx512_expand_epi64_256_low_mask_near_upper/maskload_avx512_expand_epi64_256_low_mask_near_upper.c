#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 4 * sizeof(long long));
    for (int i = 0; i < 4; i++)
        buf[i] = 0xABABABAB;
    __m256i src = _mm256_set1_epi64x(-1);
    __mmask8 mask = 0x03; /* only low elements active */
    __m256i r = _mm256_mask_expandloadu_epi64(src, mask, buf + 2);
    long long* out = zgc_aligned_alloc(64, 4 * sizeof(long long));
    _mm256_storeu_si256((__m256i*)out, r);
    ZASSERT(out[0] == 0xABABABAB);
    ZASSERT(out[1] == 0xABABABAB);
    ZASSERT(out[2] == -1);
    ZASSERT(out[3] == -1);
    zprintf("expand epi64 256 low mask near upper test passed!\n");
    return 0;
}
