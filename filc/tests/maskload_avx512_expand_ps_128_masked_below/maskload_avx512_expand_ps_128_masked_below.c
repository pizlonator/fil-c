#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 4; i++) buf[i] = i;
    float* out = zgc_aligned_alloc(64, 16);
    __m128 src = _mm_set1_ps(-1.0f);
    __m128 r;
    __mmask8 mask = 0x0C;
    r = _mm_mask_expandloadu_ps(src, mask, buf);
    _mm_storeu_ps(out, r);
    ZASSERT(out[0] == -1.0f);
    ZASSERT(out[1] == -1.0f);
    ZASSERT(out[2] == 0);
    ZASSERT(out[3] == 1);

    zprintf("expandload masked below test passed!\n");
    return 0;
}
