#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    /* Above-bounds masking: ptr = buf + 48, mask = 0x000F (4 bits).
       popcount = 4, access = 16 bytes. [buf+48, buf+64) = in bounds. */
    int* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = i;
    
    int* out = zgc_aligned_alloc(64, 64);
    
    __m512i src = _mm512_set1_epi32(-1);
    
    __mmask16 mask = 0x000F;
    __m512i result = _mm512_mask_expandloadu_epi32(src, mask, buf + 12);
    _mm512_storeu_si512((__m512i*)out, result);
    
    /* Elements 0-3 loaded from buf[12..15] = {12, 13, 14, 15} */
    ZASSERT(out[0] == 12);
    ZASSERT(out[1] == 13);
    ZASSERT(out[2] == 14);
    ZASSERT(out[3] == 15);
    /* Elements 4-15 are passthru (-1) */
    for (int i = 4; i < 16; i++)
        ZASSERT(out[i] == -1);
    
    zprintf("expandload masked above test passed!\n");
    return 0;
}
