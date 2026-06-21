#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 4 * sizeof(float));
    __m128 src = _mm_set1_ps(-1.0f);
    __mmask8 mask = 0x00;
    __m128 r = _mm_mask_expandloadu_ps(src, mask, buf + 64);
    float* out = zgc_aligned_alloc(64, 4 * sizeof(float));
    _mm_storeu_ps(out, r);
    for (int i = 0; i < 4; i++)
        ZASSERT(out[i] == -1.0f);
    zprintf("expand ps 128 zero mask oob test passed!\n");
    return 0;
}
