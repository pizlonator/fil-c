#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 32);
    int* out = zgc_aligned_alloc(64, 32);
    __m256i src = _mm256_set1_epi32(-1);
    __mmask8 mask = 0xFF;
    __m256i r = _mm256_mask_expandloadu_epi32(src, mask, buf + 8);
    _mm256_storeu_si256((__m256i*)out, r);
    zprintf("Should have failed!\n");
    return 0;
}
