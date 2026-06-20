#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw"))) int main()
{
    char* buf = zgc_aligned_alloc(32, 32);
    
    /* _mm512_set_epi16 takes args in reverse order (e31 first, e0 last).
       This sets element 0=0, element 1=1, ..., element 31=31. */
    __m512i v = _mm512_set_epi16(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
                                 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    __mmask32 mask = 0xFFFFFFFF;
    
    /* ptr = buf - 32, all mask on - all 32 bytes OOB */
    _mm512_mask_cvtsepi16_storeu_epi8(buf - 32, mask, v);
    
    zprintf("Should have failed!\n");
    return 0;
}
