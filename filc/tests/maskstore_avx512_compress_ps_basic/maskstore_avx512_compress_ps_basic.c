#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = 0.f;

    __m512 v = _mm512_setr_ps(0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f,
                              8.f, 9.f, 10.f, 11.f, 12.f, 13.f, 14.f, 15.f);

    /* Test 1: full mask (0xFFFF) - packs all 16 floats */
    __mmask16 mask = 0xFFFF;
    _mm512_mask_compressstoreu_ps(buf, mask, v);
    for (int i = 0; i < 16; i++)
        ZASSERT(buf[i] == (float)i);

    /* Test 2: zero mask - no access, buf unchanged */
    for (int i = 0; i < 16; i++) buf[i] = 42.f;
    mask = 0;
    _mm512_mask_compressstoreu_ps(buf, mask, v);
    for (int i = 0; i < 16; i++)
        ZASSERT(buf[i] == 42.f);

    /* Test 3: partial mask (0x00FF = 8 bits) - packs elements 0-7 */
    for (int i = 0; i < 16; i++) buf[i] = 0.f;
    mask = 0x00FF;
    _mm512_mask_compressstoreu_ps(buf, mask, v);
    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i] == (float)i);
    for (int i = 8; i < 16; i++)
        ZASSERT(buf[i] == 0.f);

    zprintf("compress ps basic test passed!\n");
    return 0;
}
