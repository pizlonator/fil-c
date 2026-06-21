#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 8 * sizeof(double));
    for (int i = 0; i < 8; i++)
        buf[i] = 0xABp0;
    __m512d src = _mm512_set1_pd(-1.0);
    __mmask8 mask = 0x0F; /* only low elements active */
    __m512d r = _mm512_mask_expandloadu_pd(src, mask, buf + 4);
    double* out = zgc_aligned_alloc(64, 8 * sizeof(double));
    _mm512_storeu_pd(out, r);
    ZASSERT(out[0] == 0xABp0);
    ZASSERT(out[1] == 0xABp0);
    ZASSERT(out[2] == 0xABp0);
    ZASSERT(out[3] == 0xABp0);
    ZASSERT(out[4] == -1.0);
    ZASSERT(out[5] == -1.0);
    ZASSERT(out[6] == -1.0);
    ZASSERT(out[7] == -1.0);
    zprintf("expand pd 512 low mask near upper test passed!\n");
    return 0;
}
