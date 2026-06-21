#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 4 * sizeof(float));
    for (int i = 0; i < 4; i++)
        buf[i] = 0xABp0;
    __m128 src = _mm_set1_ps(-1.0f);
    __mmask8 mask = 0x03; /* only low elements active */
    __m128 r = _mm_mask_expandloadu_ps(src, mask, buf + 2);
    float* out = zgc_aligned_alloc(64, 4 * sizeof(float));
    _mm_storeu_ps(out, r);
    ZASSERT(out[0] == 0xABp0);
    ZASSERT(out[1] == 0xABp0);
    ZASSERT(out[2] == -1.0f);
    ZASSERT(out[3] == -1.0f);
    zprintf("expand ps 128 low mask near upper test passed!\n");
    return 0;
}
