#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(2, 2);
    
    __m128i v = _mm_set_epi64x(0x41, 0x42);
    __mmask8 mask = 0x3;
    
    /* ptr = buf - 2, all mask on - both bytes OOB */
    _mm_mask_cvtepi64_storeu_epi8(buf - 2, mask, v);
    
    zprintf("Should have failed!\n");
    return 0;
}
