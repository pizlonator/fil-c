#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 8; i++) buf[i] = 0.;

    __m512d v = _mm512_setr_pd(0., 1., 2., 3., 4., 5., 6., 7.);

    /* Test 1: full mask (0xFF) - packs all 8 doubles */
    __mmask8 mask = 0xFF;
    _mm512_mask_compressstoreu_pd(buf, mask, v);
    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i] == (double)i);

    /* Test 2: zero mask - no access, buf unchanged */
    for (int i = 0; i < 8; i++) buf[i] = 42.;
    mask = 0;
    _mm512_mask_compressstoreu_pd(buf, mask, v);
    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i] == 42.);

    /* Test 3: partial mask (0x0F = 4 bits) - packs elements 0-3 */
    for (int i = 0; i < 8; i++) buf[i] = 0.;
    mask = 0x0F;
    _mm512_mask_compressstoreu_pd(buf, mask, v);
    for (int i = 0; i < 4; i++)
        ZASSERT(buf[i] == (double)i);
    for (int i = 4; i < 8; i++)
        ZASSERT(buf[i] == 0.);

    zprintf("compress pd basic test passed!\n");
    return 0;
}
