#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = i;
    int* out = zgc_aligned_alloc(64, 64);
    __m512i src = _mm512_set1_epi32(-1);
    __m512i r;
    __mmask16 mask = 0xFF00;
    r = _mm512_mask_expandloadu_epi32(src, mask, buf);
    _mm512_storeu_si512((__m512i*)out, r);
    ZASSERT(out[0] == -1);
    ZASSERT(out[1] == -1);
    ZASSERT(out[2] == -1);
    ZASSERT(out[3] == -1);
    ZASSERT(out[4] == -1);
    ZASSERT(out[5] == -1);
    ZASSERT(out[6] == -1);
    ZASSERT(out[7] == -1);
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
