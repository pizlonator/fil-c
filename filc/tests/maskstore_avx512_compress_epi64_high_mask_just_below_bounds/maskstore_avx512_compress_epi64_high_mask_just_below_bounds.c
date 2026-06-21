#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 64);

    __m512i v = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);
    __mmask8 mask = 0xF0; /* only high elements active */

    /* For compressstore the first active element is written at ptr.
       Access starts at buf + -4, which is OOB. */
    _mm512_mask_compressstoreu_epi64(buf + -4, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
