#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    float* buf = (float*)zgc_aligned_alloc(64, 4 * sizeof(float));
    for (int i = 0; i < 4; i++)
        buf[i] = (float)i;
    float* out = (float*)zgc_aligned_alloc(64, 4 * sizeof(float));
    for (int i = 0; i < 4; i++)
        out[i] = (float)-1;
    __m128 src = _mm_set1_ps(-1.0f);
    __mmask8 mask = 0xF0;
    __m128 r = _mm_mask_expandloadu_ps(src, mask, buf + 16);
    _mm_storeu_ps(out, r);
    for (int i = 0; i < 4; i++)
        ZASSERT(out[i] == (float)-1);
    zprintf("expand ps 128 truncated zero mask oob test passed!\n");
    return 0;
}
