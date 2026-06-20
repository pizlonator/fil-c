#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(16, 16);
    
    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __mmask16 mask = 0xFFFF;
    
    /* ptr = buf - 16, all mask on - all 16 bytes OOB */
    _mm512_mask_cvtepi32_storeu_epi8(buf - 16, mask, v);
    
    zprintf("Should have failed!\n");
    return 0;
}
