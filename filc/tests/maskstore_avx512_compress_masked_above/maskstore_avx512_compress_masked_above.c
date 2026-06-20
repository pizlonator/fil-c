#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    /* Above-bounds masking: ptr = buf + 48, mask = 0x000F (4 bits).
       popcount = 4, access = 16 bytes. [buf+48, buf+64) = in bounds.
       Full word would be 64 bytes at [buf+48, buf+112) = OOB, but only 16 accessed. */
    int* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = 0;
    
    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    
    __mmask16 mask = 0x000F;
    _mm512_mask_compressstoreu_epi32(buf + 12, mask, v);
    
    /* Active elements 0-3 packed at buf+12 */
    ZASSERT(buf[12] == 0);
    ZASSERT(buf[13] == 1);
    ZASSERT(buf[14] == 2);
    ZASSERT(buf[15] == 3);
    for (int i = 0; i < 12; i++)
        ZASSERT(buf[i] == 0);
    
    zprintf("compressstore masked above test passed!\n");
    return 0;
}
