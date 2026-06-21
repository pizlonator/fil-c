#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = i;
    float* out = zgc_aligned_alloc(64, 64);
    __m512 src = _mm512_set1_ps(-1.0f);
    __m512 r;
    __mmask16 mask = 0x00FF;
    r = _mm512_mask_expandloadu_ps(src, mask, buf + 8);
    _mm512_storeu_ps(out, r);
    ZASSERT(out[0] == 8);
    ZASSERT(out[1] == 9);
    ZASSERT(out[2] == 10);
    ZASSERT(out[3] == 11);
    ZASSERT(out[4] == 12);
    ZASSERT(out[5] == 13);
    ZASSERT(out[6] == 14);
    ZASSERT(out[7] == 15);
    ZASSERT(out[8] == -1.0f);
    ZASSERT(out[9] == -1.0f);
    ZASSERT(out[10] == -1.0f);
    ZASSERT(out[11] == -1.0f);
    ZASSERT(out[12] == -1.0f);
    ZASSERT(out[13] == -1.0f);
    ZASSERT(out[14] == -1.0f);
    ZASSERT(out[15] == -1.0f);

    zprintf("expandload masked above test passed!\n");
    return 0;
}
