#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 16 * sizeof(float));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 16 * sizeof(float); i++)
        bytes[i] = 0xAB;
    __m512 v = _mm512_setr_ps(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __mmask16 mask = 0x00FF; /* only low elements active */
    _mm512_mask_compressstoreu_ps(buf + 8, mask, v);
    for (size_t i = 0; i < 8 * sizeof(float); i++)
        ZASSERT(bytes[i] == 0xAB);
    ZASSERT(buf[8] == 0);
    ZASSERT(buf[9] == 1);
    ZASSERT(buf[10] == 2);
    ZASSERT(buf[11] == 3);
    ZASSERT(buf[12] == 4);
    ZASSERT(buf[13] == 5);
    ZASSERT(buf[14] == 6);
    ZASSERT(buf[15] == 7);
    zprintf("compress ps 512 low mask near upper test passed!\n");
    return 0;
}
