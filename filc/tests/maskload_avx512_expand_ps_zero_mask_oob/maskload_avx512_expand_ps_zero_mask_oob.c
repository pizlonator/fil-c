#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++)
        buf[i] = -1.0f;
    __m512 src = _mm512_set1_ps(-1.0f);

    /* Zero mask with an OOB pointer must not access memory. */
    __mmask16 mask = 0;
    __m512 r = _mm512_mask_expandloadu_ps(src, mask, buf + 16);
    float* out = zgc_aligned_alloc(64, 64);
    _mm512_storeu_ps(out, r);

    for (int i = 0; i < 16; i++)
        ZASSERT(out[i] == -1.0f);

    zprintf("expand ps zero mask oob test passed!\n");
    return 0;
}
