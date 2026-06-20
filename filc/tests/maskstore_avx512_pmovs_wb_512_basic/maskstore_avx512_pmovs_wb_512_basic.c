#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw"))) int main()
{
    /* Test _mm512_mask_cvtsepi16_storeu_epi8 (pmovs_wb_mem_512: 32x i16 -> 32x i8, signed sat) */
    char* buf = zgc_aligned_alloc(32, 32);
    for (int i = 0; i < 32; i++) buf[i] = 0;
    
    /* _mm512_set_epi16 takes args in reverse order (e31 first, e0 last).
       This sets element 0=0, element 1=1, ..., element 31=31. */
    __m512i v = _mm512_set_epi16(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
                                 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    
    __mmask32 mask = 0xFFFFFFFF;
    _mm512_mask_cvtsepi16_storeu_epi8(buf, mask, v);
    
    /* Values 0-31 fit in signed i8, so no saturation */
    for (int i = 0; i < 32; i++)
        ZASSERT(buf[i] == (char)i);
    
    zprintf("pmovs_wb_512 basic test passed!\n");
    return 0;
}
