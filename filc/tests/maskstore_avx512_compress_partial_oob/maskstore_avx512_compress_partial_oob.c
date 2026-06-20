#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);
    
    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __mmask16 mask = 0xFFFF;
    
    /* ptr = buf + 60, all mask on. popcount=16, access=64 bytes.
       [buf+60, buf+124) - 4 bytes in-bounds, 60 bytes OOB. Fails. */
    _mm512_mask_compressstoreu_epi32(buf + 15, mask, v);
    
    zprintf("Should have failed!\n");
    return 0;
}
