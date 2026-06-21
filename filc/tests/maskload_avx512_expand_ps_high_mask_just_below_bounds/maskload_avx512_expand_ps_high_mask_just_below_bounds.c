#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);
    __m512 src = _mm512_set1_ps(-1.0f);
    __mmask16 mask = 0xFF00; /* only high elements active */

    /* For expandload the first active element is read from ptr.
       Access starts at buf + -8, which is OOB. */
    __m512 r = _mm512_mask_expandloadu_ps(src, mask, buf + -8);
    _mm512_storeu_ps(buf, r);

    zprintf("Should have failed!\n");
    return 0;
}
