#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    /* Test _mm512_mask_expandloadu_epi32 (expandload: 16x i32, contiguous) */
    int* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = i;
    
    int* out = zgc_aligned_alloc(64, 64);
    
    __m512i src = _mm512_set1_epi32(-1);
    
    /* Test 1: partial mask (0x00FF = 8 bits) - reads 8 elements from buf */
    __mmask16 mask = 0x00FF;
    __m512i result = _mm512_mask_expandloadu_epi32(src, mask, buf);
    _mm512_storeu_si512((__m512i*)out, result);
    
    for (int i = 0; i < 8; i++)
        ZASSERT(out[i] == i);
    for (int i = 8; i < 16; i++)
        ZASSERT(out[i] == -1);
    
    /* Test 2: full mask (0xFFFF) - reads all 16 elements */
    mask = 0xFFFF;
    result = _mm512_mask_expandloadu_epi32(src, mask, buf);
    _mm512_storeu_si512((__m512i*)out, result);
    for (int i = 0; i < 16; i++)
        ZASSERT(out[i] == i);
    
    /* Test 3: zero mask - no access, all passthru */
    mask = 0;
    result = _mm512_mask_expandloadu_epi32(src, mask, buf);
    _mm512_storeu_si512((__m512i*)out, result);
    for (int i = 0; i < 16; i++)
        ZASSERT(out[i] == -1);
    
    zprintf("expandload basic test passed!\n");
    return 0;
}
