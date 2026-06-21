#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = 42;
    __m512i src = _mm512_set1_epi32(-1);

    /* Zero mask with an OOB pointer must not access memory. */
    __mmask16 mask = 0;
    __m512i r = _mm512_mask_expandloadu_epi32(src, mask, buf + 16);
    _mm512_storeu_si512((__m512i*)buf, r);

    for (int i = 0; i < 16; i++)
        ZASSERT(buf[i] == -1);

    zprintf("expand zero mask oob test passed!\n");
    return 0;
}
