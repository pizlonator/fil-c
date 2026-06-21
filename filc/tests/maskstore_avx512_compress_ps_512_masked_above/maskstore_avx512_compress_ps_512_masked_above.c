#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = 0;
    __m512 v = _mm512_setr_ps(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f);
    __mmask16 mask = 0x00FF;
    _mm512_mask_compressstoreu_ps(buf + 8, mask, v);
    ZASSERT(buf[8] == 0);
    ZASSERT(buf[9] == 1);
    ZASSERT(buf[10] == 2);
    ZASSERT(buf[11] == 3);
    ZASSERT(buf[12] == 4);
    ZASSERT(buf[13] == 5);
    ZASSERT(buf[14] == 6);
    ZASSERT(buf[15] == 7);
    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i] == 0);

    zprintf("compressstore masked above test passed!\n");
    return 0;
}
