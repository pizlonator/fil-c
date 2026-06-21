#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 8; i++) buf[i] = i;
    float* out = zgc_aligned_alloc(64, 32);
    __m256 src = _mm256_set1_ps(-1.0f);
    __m256 r;
    __mmask8 mask;
    __mmask8 odd_mask = 0xAA;

    /* Partial odd mask: active lanes are odd-numbered positions. */
    mask = odd_mask;
    r = _mm256_mask_expandloadu_ps(src, mask, buf);
    _mm256_storeu_ps(out, r);
    ZASSERT(out[1] == 0);
    ZASSERT(out[3] == 1);
    ZASSERT(out[5] == 2);
    ZASSERT(out[7] == 3);
    ZASSERT(out[0] == -1.0f);
    ZASSERT(out[2] == -1.0f);
    ZASSERT(out[4] == -1.0f);
    ZASSERT(out[6] == -1.0f);

    mask = 0xFF;
    r = _mm256_mask_expandloadu_ps(src, mask, buf);
    _mm256_storeu_ps(out, r);
    for (int i = 0; i < 8; i++)
        ZASSERT(out[i] == i);

    mask = 0;
    r = _mm256_mask_expandloadu_ps(src, mask, buf);
    _mm256_storeu_ps(out, r);
    for (int i = 0; i < 8; i++)
        ZASSERT(out[i] == -1.0f);

    zprintf("expandload basic test passed!\n");
    return 0;
}
