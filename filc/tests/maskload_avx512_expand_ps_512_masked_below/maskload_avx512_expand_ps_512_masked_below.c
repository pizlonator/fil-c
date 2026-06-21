#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = i;
    float* out = zgc_aligned_alloc(64, 64);
    __m512 src = _mm512_set1_ps(-1.0f);
    __m512 r;
    __mmask16 mask = 0xFF00;
    r = _mm512_mask_expandloadu_ps(src, mask, buf);
    _mm512_storeu_ps(out, r);
    ZASSERT(out[0] == -1.0f);
    ZASSERT(out[1] == -1.0f);
    ZASSERT(out[2] == -1.0f);
    ZASSERT(out[3] == -1.0f);
    ZASSERT(out[4] == -1.0f);
    ZASSERT(out[5] == -1.0f);
    ZASSERT(out[6] == -1.0f);
    ZASSERT(out[7] == -1.0f);
    ZASSERT(out[8] == 0);
    ZASSERT(out[9] == 1);
    ZASSERT(out[10] == 2);
    ZASSERT(out[11] == 3);
    ZASSERT(out[12] == 4);
    ZASSERT(out[13] == 5);
    ZASSERT(out[14] == 6);
    ZASSERT(out[15] == 7);

    zprintf("expandload masked below test passed!\n");
    return 0;
}
