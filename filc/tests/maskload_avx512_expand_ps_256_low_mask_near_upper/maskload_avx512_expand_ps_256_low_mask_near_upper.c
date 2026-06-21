#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 8 * sizeof(float));
    for (int i = 0; i < 8; i++)
        buf[i] = 0xABp0;
    __m256 src = _mm256_set1_ps(-1.0f);
    __mmask8 mask = 0x0F; /* only low elements active */
    __m256 r = _mm256_mask_expandloadu_ps(src, mask, buf + 4);
    float* out = zgc_aligned_alloc(64, 8 * sizeof(float));
    _mm256_storeu_ps(out, r);
    ZASSERT(out[0] == 0xABp0);
    ZASSERT(out[1] == 0xABp0);
    ZASSERT(out[2] == 0xABp0);
    ZASSERT(out[3] == 0xABp0);
    ZASSERT(out[4] == -1.0f);
    ZASSERT(out[5] == -1.0f);
    ZASSERT(out[6] == -1.0f);
    ZASSERT(out[7] == -1.0f);
    zprintf("expand ps 256 low mask near upper test passed!\n");
    return 0;
}
