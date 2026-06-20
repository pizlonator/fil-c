#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    /* Test _mm512_mask_compressstoreu_epi32 (compressstore: 16x i32, contiguous) */
    int* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = 0;
    
    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    
    /* Test 1: partial mask (0x00FF = 8 bits) - packs elements 0-7 at buf */
    __mmask16 mask = 0x00FF;
    _mm512_mask_compressstoreu_epi32(buf, mask, v);
    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i] == i);
    for (int i = 8; i < 16; i++)
        ZASSERT(buf[i] == 0);
    
    /* Test 2: full mask (0xFFFF) - packs all 16 elements */
    for (int i = 0; i < 16; i++) buf[i] = 0;
    mask = 0xFFFF;
    _mm512_mask_compressstoreu_epi32(buf, mask, v);
    for (int i = 0; i < 16; i++)
        ZASSERT(buf[i] == i);
    
    /* Test 3: zero mask - no access */
    for (int i = 0; i < 16; i++) buf[i] = 42;
    mask = 0;
    _mm512_mask_compressstoreu_epi32(buf, mask, v);
    for (int i = 0; i < 16; i++)
        ZASSERT(buf[i] == 42);
    
    zprintf("compressstore basic test passed!\n");
    return 0;
}
