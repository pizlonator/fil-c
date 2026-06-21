#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 64);

    __m512i v = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);
    __mmask8 mask = 0x01; /* only element 0 active */

    /* popcount=1 -> one element written at buf + 8, just above upper bound. */
    _mm512_mask_compressstoreu_epi64(buf + 8, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
