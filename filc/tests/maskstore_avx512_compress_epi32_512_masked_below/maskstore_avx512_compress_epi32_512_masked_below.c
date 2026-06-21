#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = 0;
    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __mmask16 mask = 0xFF00;
    _mm512_mask_compressstoreu_epi32(buf, mask, v);
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
