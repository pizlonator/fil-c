#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);
    
    __m512i src = _mm512_set1_epi32(-1);
    __mmask16 mask = 0xFFFF;
    
    /* ptr = buf + 64, all mask on - all 64 bytes OOB */
    __m512i result = _mm512_mask_expandloadu_epi32(src, mask, buf + 16);
    /* Store result to keep the load live (prevents dead-load elimination). */
    _mm512_storeu_si512((__m512i*)buf, result);
    
    zprintf("Should have failed!\n");
    return 0;
}
