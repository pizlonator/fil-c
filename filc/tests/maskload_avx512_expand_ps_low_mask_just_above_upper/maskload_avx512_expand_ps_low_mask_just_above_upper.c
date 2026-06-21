#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);
    __m512 src = _mm512_set1_ps(-1.0f);
    __mmask16 mask = 0x0001; /* only element 0 active */

    /* popcount=1 -> one element read at buf + 16, just above upper bound. */
    __m512 r = _mm512_mask_expandloadu_ps(src, mask, buf + 16);
    _mm512_storeu_ps(buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
