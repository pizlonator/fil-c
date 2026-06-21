#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 2; i++) buf[i] = i;
    double* out = zgc_aligned_alloc(64, 16);
    __m128d src = _mm_set1_pd(-1.0);
    __m128d r;
    __mmask8 mask = 0x02;
    r = _mm_mask_expandloadu_pd(src, mask, buf);
    _mm_storeu_pd(out, r);
    ZASSERT(out[0] == -1.0);
    ZASSERT(out[1] == 0);

    zprintf("expandload masked below test passed!\n");
    return 0;
}
