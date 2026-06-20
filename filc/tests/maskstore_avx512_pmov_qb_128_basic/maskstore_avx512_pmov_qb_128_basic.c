#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    /* Test _mm_mask_cvtepi64_storeu_epi8 (pmov_qb_mem_128: 2x i64 -> 2x i8)
       This tests the FixMask truncation path (i8 mask, N=2). */
    char* buf = zgc_aligned_alloc(2, 2);
    buf[0] = 0;
    buf[1] = 0;
    
    /* _mm_set_epi64x(e1, e0): element 0 = e0, element 1 = e1 */
    __m128i v = _mm_set_epi64x(0x41, 0x42);
    
    /* Full mask: both elements */
    __mmask8 mask = 0x3;
    _mm_mask_cvtepi64_storeu_epi8(buf, mask, v);
    
    ZASSERT(buf[0] == (char)0x42);
    ZASSERT(buf[1] == (char)0x41);
    
    zprintf("pmov_qb_128 basic test passed!\n");
    return 0;
}
