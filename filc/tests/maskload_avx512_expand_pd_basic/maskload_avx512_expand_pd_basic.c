#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 8; i++) buf[i] = (double)i;

    double* out = zgc_aligned_alloc(64, 64);
    __m512d src = _mm512_set1_pd(-1.0);

    /* Test 1: full mask (0xFF) - reads all 8 doubles */
    __mmask8 mask = 0xFF;
    __m512d result = _mm512_mask_expandloadu_pd(src, mask, buf);
    _mm512_storeu_pd(out, result);
    for (int i = 0; i < 8; i++)
        ZASSERT(out[i] == (double)i);

    /* Test 2: zero mask - no access, all passthru */
    mask = 0;
    result = _mm512_mask_expandloadu_pd(src, mask, buf);
    _mm512_storeu_pd(out, result);
    for (int i = 0; i < 8; i++)
        ZASSERT(out[i] == -1.0);

    /* Test 3: partial mask (0x0F = 4 bits) - reads 4 elements */
    mask = 0x0F;
    result = _mm512_mask_expandloadu_pd(src, mask, buf);
    _mm512_storeu_pd(out, result);
    for (int i = 0; i < 4; i++)
        ZASSERT(out[i] == (double)i);
    for (int i = 4; i < 8; i++)
        ZASSERT(out[i] == -1.0);

    zprintf("expand pd basic test passed!\n");
    return 0;
}
