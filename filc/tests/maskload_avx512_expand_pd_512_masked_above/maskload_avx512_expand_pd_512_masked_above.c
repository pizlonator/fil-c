#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 8; i++) buf[i] = i;
    double* out = zgc_aligned_alloc(64, 64);
    __m512d src = _mm512_set1_pd(-1.0);
    __m512d r;
    __mmask8 mask = 0x0F;
    r = _mm512_mask_expandloadu_pd(src, mask, buf + 4);
    _mm512_storeu_pd(out, r);
    ZASSERT(out[0] == 4);
    ZASSERT(out[1] == 5);
    ZASSERT(out[2] == 6);
    ZASSERT(out[3] == 7);
    ZASSERT(out[4] == -1.0);
    ZASSERT(out[5] == -1.0);
    ZASSERT(out[6] == -1.0);
    ZASSERT(out[7] == -1.0);

    zprintf("expandload masked above test passed!\n");
    return 0;
}
