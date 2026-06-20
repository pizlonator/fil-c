#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    /* Below-bounds masking: ptr = buf - 1, mask = 0x2 (element 1 only).
       Element 1 at ptr+1 = buf (in bounds). Element 0 at ptr = buf-1 (OOB, masked off). */
    char* buf = zgc_aligned_alloc(2, 2);
    buf[0] = 0;
    buf[1] = 0;
    
    __m128i v = _mm_set_epi64x(0x41, 0x42);
    
    __mmask8 mask = 0x2;
    _mm_mask_cvtepi64_storeu_epi8(buf - 1, mask, v);
    
    /* Element 1 stored at buf[0]: value 0x41 */
    ZASSERT(buf[0] == (char)0x41);
    ZASSERT(buf[1] == 0);
    
    zprintf("pmov_qb_128 masked below test passed!\n");
    return 0;
}
