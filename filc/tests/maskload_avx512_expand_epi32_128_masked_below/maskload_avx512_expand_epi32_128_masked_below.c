#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 4; i++) buf[i] = i;
    int* out = zgc_aligned_alloc(64, 16);
    __m128i src = _mm_set1_epi32(-1);
    __m128i r;
    __mmask8 mask = 0x0C;
    r = _mm_mask_expandloadu_epi32(src, mask, buf);
    _mm_storeu_si128((__m128i*)out, r);
    ZASSERT(out[0] == -1);
    ZASSERT(out[1] == -1);
    ZASSERT(out[2] == 0);
    ZASSERT(out[3] == 1);

    zprintf("expandload masked below test passed!\n");
    return 0;
}
