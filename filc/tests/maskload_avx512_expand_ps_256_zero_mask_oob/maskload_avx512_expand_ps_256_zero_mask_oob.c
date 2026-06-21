#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 8 * sizeof(float));
    __m256 src = _mm256_set1_ps(-1.0f);
    __mmask8 mask = 0x00;
    __m256 r = _mm256_mask_expandloadu_ps(src, mask, buf + 64);
    float* out = zgc_aligned_alloc(64, 8 * sizeof(float));
    _mm256_storeu_ps(out, r);
    for (int i = 0; i < 8; i++)
        ZASSERT(out[i] == -1.0f);
    zprintf("expand ps 256 zero mask oob test passed!\n");
    return 0;
}
