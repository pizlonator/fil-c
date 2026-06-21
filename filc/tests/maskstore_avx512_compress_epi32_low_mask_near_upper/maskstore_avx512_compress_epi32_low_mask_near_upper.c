#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++)
        buf[i] = 0xABABABAB;
    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

    /* Only element 0 active. popcount=1 -> 4 bytes written at buf+60..buf+63. */
    __mmask16 mask = 0x0001;
    _mm512_mask_compressstoreu_epi32(buf + 15, mask, v);
    ZASSERT(buf[15] == 0);
    for (int i = 0; i < 15; i++)
        ZASSERT(buf[i] == 0xABABABAB);

    zprintf("compress low mask near upper test passed!\n");
    return 0;
}
