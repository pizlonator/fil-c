#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 4; i++) buf[i] = i;
    long long* out = zgc_aligned_alloc(64, 32);
    __m256i src = _mm256_set1_epi64x(-1);
    __m256i r;
    __mmask8 mask = 0x0C;
    r = _mm256_mask_expandloadu_epi64(src, mask, buf);
    _mm256_storeu_si256((__m256i*)out, r);
    ZASSERT(out[0] == -1);
    ZASSERT(out[1] == -1);
    ZASSERT(out[2] == 0);
    ZASSERT(out[3] == 1);

    zprintf("expandload masked below test passed!\n");
    return 0;
}
