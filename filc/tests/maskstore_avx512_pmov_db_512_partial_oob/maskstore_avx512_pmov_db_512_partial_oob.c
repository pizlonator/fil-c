#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(16, 16);
    
    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __mmask16 mask = 0xFFFF;
    
    /* ptr = buf - 4, all mask on.
       Elements 0-3 at buf-4..buf-1 (OOB, mask-enabled) -> should fail.
       Elements 4-15 at buf..buf+11 (in-bounds, mask-enabled). */
    _mm512_mask_cvtepi32_storeu_epi8(buf - 4, mask, v);
    
    zprintf("Should have failed!\n");
    return 0;
}
