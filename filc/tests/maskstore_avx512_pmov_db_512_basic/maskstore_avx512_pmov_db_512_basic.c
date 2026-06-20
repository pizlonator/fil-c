#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    /* Test _mm512_mask_cvtepi32_storeu_epi8 (pmov_db_mem_512: 16x i32 -> 16x i8) */
    char* buf = zgc_aligned_alloc(16, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0;
    
    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    
    /* Test 1: full mask - store all 16 bytes */
    __mmask16 mask = 0xFFFF;
    _mm512_mask_cvtepi32_storeu_epi8(buf, mask, v);
    for (int i = 0; i < 16; i++)
        ZASSERT(buf[i] == (char)(i & 0xFF));
    
    /* Test 2: partial mask (0xAAAA = bits 1,3,5,...,15) - store only odd elements */
    for (int i = 0; i < 16; i++) buf[i] = 0;
    mask = 0xAAAA;
    _mm512_mask_cvtepi32_storeu_epi8(buf, mask, v);
    for (int i = 0; i < 16; i++) {
        if (i % 2 == 1)
            ZASSERT(buf[i] == (char)(i & 0xFF));
        else
            ZASSERT(buf[i] == 0);
    }
    
    /* Test 3: zero mask - no access */
    for (int i = 0; i < 16; i++) buf[i] = 42;
    mask = 0;
    _mm512_mask_cvtepi32_storeu_epi8(buf, mask, v);
    for (int i = 0; i < 16; i++)
        ZASSERT(buf[i] == 42);
    
    zprintf("pmov_db_512 basic test passed!\n");
    return 0;
}
