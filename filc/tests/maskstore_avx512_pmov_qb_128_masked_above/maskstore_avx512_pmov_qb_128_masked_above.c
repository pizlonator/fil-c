#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    /* Above-bounds masking: allocate 16 bytes, ptr = buf + 15, mask = 0x1 (element 0 only).
       Since Fil-C rounds allocations up to 16 bytes, a 2-byte alloc would still have
       16 accessible bytes. So we use a 16-byte allocation and point at offset 15 so
       that element 1 (at ptr+1 = buf+16) is genuinely OOB.
       Element 0 at ptr+0 = buf+15 (in bounds, last byte of 16-byte allocation). */
    char* buf = zgc_aligned_alloc(16, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0;
    
    __m128i v = _mm_set_epi64x(0x41, 0x42);
    
    __mmask8 mask = 0x1;
    _mm_mask_cvtepi64_storeu_epi8(buf + 15, mask, v);
    
    /* Element 0 stored at buf[15]: value 0x42 */
    ZASSERT(buf[15] == (char)0x42);
    for (int i = 0; i < 15; i++)
        ZASSERT(buf[i] == 0);
    
    zprintf("pmov_qb_128 masked above test passed!\n");
    return 0;
}
