#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 8; i++) buf[i] = i;
    int* out = zgc_aligned_alloc(64, 32);
    __m256i src = _mm256_set1_epi32(-1);
    __m256i r;
    __mmask8 mask = 0x0F;
    r = _mm256_mask_expandloadu_epi32(src, mask, buf + 4);
    _mm256_storeu_si256((__m256i*)out, r);
    ZASSERT(out[0] == 4);
    ZASSERT(out[1] == 5);
    ZASSERT(out[2] == 6);
    ZASSERT(out[3] == 7);
    ZASSERT(out[4] == -1);
    ZASSERT(out[5] == -1);
    ZASSERT(out[6] == -1);
    ZASSERT(out[7] == -1);

    zprintf("expandload masked above test passed!\n");
    return 0;
}
