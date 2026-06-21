#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 8; i++) buf[i] = i;
    float* out = zgc_aligned_alloc(64, 32);
    __m256 src = _mm256_set1_ps(-1.0f);
    __m256 r;
    __mmask8 mask = 0xF0;
    r = _mm256_mask_expandloadu_ps(src, mask, buf);
    _mm256_storeu_ps(out, r);
    ZASSERT(out[0] == -1.0f);
    ZASSERT(out[1] == -1.0f);
    ZASSERT(out[2] == -1.0f);
    ZASSERT(out[3] == -1.0f);
    ZASSERT(out[4] == 0);
    ZASSERT(out[5] == 1);
    ZASSERT(out[6] == 2);
    ZASSERT(out[7] == 3);

    zprintf("expandload masked below test passed!\n");
    return 0;
}
