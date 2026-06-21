#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = 0;
    __m512 v = _mm512_setr_ps(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f);
    __mmask16 mask = 0xFF00;
    _mm512_mask_compressstoreu_ps(buf, mask, v);
    ZASSERT(buf[0] == 8);
    ZASSERT(buf[1] == 9);
    ZASSERT(buf[2] == 10);
    ZASSERT(buf[3] == 11);
    ZASSERT(buf[4] == 12);
    ZASSERT(buf[5] == 13);
    ZASSERT(buf[6] == 14);
    ZASSERT(buf[7] == 15);
    for (int i = 8; i < 16; i++)
        ZASSERT(buf[i] == 0);

    zprintf("compressstore masked below test passed!\n");
    return 0;
}
