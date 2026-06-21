#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 4; i++) buf[i] = i;
    double* out = zgc_aligned_alloc(64, 32);
    __m256d src = _mm256_set1_pd(-1.0);
    __m256d r;
    __mmask8 mask = 0x03;
    r = _mm256_mask_expandloadu_pd(src, mask, buf + 2);
    _mm256_storeu_pd(out, r);
    ZASSERT(out[0] == 2);
    ZASSERT(out[1] == 3);
    ZASSERT(out[2] == -1.0);
    ZASSERT(out[3] == -1.0);

    zprintf("expandload masked above test passed!\n");
    return 0;
}
