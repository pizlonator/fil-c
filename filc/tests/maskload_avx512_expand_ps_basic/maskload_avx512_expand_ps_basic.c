#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = (float)i;

    float* out = zgc_aligned_alloc(64, 64);
    __m512 src = _mm512_set1_ps(-1.0f);

    /* Test 1: full mask (0xFFFF) - reads all 16 floats */
    __mmask16 mask = 0xFFFF;
    __m512 result = _mm512_mask_expandloadu_ps(src, mask, buf);
    _mm512_storeu_ps(out, result);
    for (int i = 0; i < 16; i++)
        ZASSERT(out[i] == (float)i);

    /* Test 2: zero mask - no access, all passthru */
    mask = 0;
    result = _mm512_mask_expandloadu_ps(src, mask, buf);
    _mm512_storeu_ps(out, result);
    for (int i = 0; i < 16; i++)
        ZASSERT(out[i] == -1.0f);

    /* Test 3: partial mask (0x00FF = 8 bits) - reads 8 elements */
    mask = 0x00FF;
    result = _mm512_mask_expandloadu_ps(src, mask, buf);
    _mm512_storeu_ps(out, result);
    for (int i = 0; i < 8; i++)
        ZASSERT(out[i] == (float)i);
    for (int i = 8; i < 16; i++)
        ZASSERT(out[i] == -1.0f);

    zprintf("expand ps basic test passed!\n");
    return 0;
}
