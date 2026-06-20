#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    /* Above-bounds masking: ptr = buf + 8, mask = 0x00FF (elements 0-7 only).
       Elements 0-7 are at ptr+0..ptr+7 = buf+8..buf+15 (in bounds).
       Elements 8-15 are at ptr+8..ptr+15 = buf+16..buf+23 (OOB, masked off). */
    char* buf = zgc_aligned_alloc(16, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0;
    
    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    
    __mmask16 mask = 0x00FF;
    _mm512_mask_cvtepi32_storeu_epi8(buf + 8, mask, v);
    
    /* Elements 0-7 stored at buf[8..15]: values 0..7 */
    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i + 8] == (char)i);
    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i] == 0);
    
    zprintf("pmov_db_512 masked above test passed!\n");
    return 0;
}
