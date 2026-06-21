#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 4; i++) buf[i] = i;
    float* out = zgc_aligned_alloc(64, 16);
    __m128 src = _mm_set1_ps(-1.0f);
    __m128 r;
    __mmask8 mask;
    __mmask8 odd_mask = 0x0A;

    /* Partial odd mask: active lanes are odd-numbered positions. */
    mask = odd_mask;
    r = _mm_mask_expandloadu_ps(src, mask, buf);
    _mm_storeu_ps(out, r);
    ZASSERT(out[1] == 0);
    ZASSERT(out[3] == 1);
    ZASSERT(out[0] == -1.0f);
    ZASSERT(out[2] == -1.0f);

    mask = 0x0F;
    r = _mm_mask_expandloadu_ps(src, mask, buf);
    _mm_storeu_ps(out, r);
    for (int i = 0; i < 4; i++)
        ZASSERT(out[i] == i);

    mask = 0;
    r = _mm_mask_expandloadu_ps(src, mask, buf);
    _mm_storeu_ps(out, r);
    for (int i = 0; i < 4; i++)
        ZASSERT(out[i] == -1.0f);

    zprintf("expandload basic test passed!\n");
    return 0;
}
