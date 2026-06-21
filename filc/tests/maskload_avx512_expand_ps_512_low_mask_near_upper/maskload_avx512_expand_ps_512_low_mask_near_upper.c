#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 16 * sizeof(float));
    for (int i = 0; i < 16; i++)
        buf[i] = 0xABp0;
    __m512 src = _mm512_set1_ps(-1.0f);
    __mmask16 mask = 0x00FF; /* only low elements active */
    __m512 r = _mm512_mask_expandloadu_ps(src, mask, buf + 8);
    float* out = zgc_aligned_alloc(64, 16 * sizeof(float));
    _mm512_storeu_ps(out, r);
    ZASSERT(out[0] == 0xABp0);
    ZASSERT(out[1] == 0xABp0);
    ZASSERT(out[2] == 0xABp0);
    ZASSERT(out[3] == 0xABp0);
    ZASSERT(out[4] == 0xABp0);
    ZASSERT(out[5] == 0xABp0);
    ZASSERT(out[6] == 0xABp0);
    ZASSERT(out[7] == 0xABp0);
    ZASSERT(out[8] == -1.0f);
    ZASSERT(out[9] == -1.0f);
    ZASSERT(out[10] == -1.0f);
    ZASSERT(out[11] == -1.0f);
    ZASSERT(out[12] == -1.0f);
    ZASSERT(out[13] == -1.0f);
    ZASSERT(out[14] == -1.0f);
    ZASSERT(out[15] == -1.0f);
    zprintf("expand ps 512 low mask near upper test passed!\n");
    return 0;
}
