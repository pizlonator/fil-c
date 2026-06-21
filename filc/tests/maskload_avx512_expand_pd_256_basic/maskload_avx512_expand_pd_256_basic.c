#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 4; i++) buf[i] = i;
    double* out = zgc_aligned_alloc(64, 32);
    __m256d src = _mm256_set1_pd(-1.0);
    __m256d r;
    __mmask8 mask;
    __mmask8 odd_mask = 0x0A;

    /* Partial odd mask: active lanes are odd-numbered positions. */
    mask = odd_mask;
    r = _mm256_mask_expandloadu_pd(src, mask, buf);
    _mm256_storeu_pd(out, r);
    ZASSERT(out[1] == 0);
    ZASSERT(out[3] == 1);
    ZASSERT(out[0] == -1.0);
    ZASSERT(out[2] == -1.0);

    mask = 0x0F;
    r = _mm256_mask_expandloadu_pd(src, mask, buf);
    _mm256_storeu_pd(out, r);
    for (int i = 0; i < 4; i++)
        ZASSERT(out[i] == i);

    mask = 0;
    r = _mm256_mask_expandloadu_pd(src, mask, buf);
    _mm256_storeu_pd(out, r);
    for (int i = 0; i < 4; i++)
        ZASSERT(out[i] == -1.0);

    zprintf("expandload basic test passed!\n");
    return 0;
}
