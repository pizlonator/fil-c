#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++) buf[i] = i;
    __m512i src = _mm512_set1_epi32(0xABABABAB);

    /* Only element 0 active. popcount=1 -> 4 bytes read at buf+60..buf+63. */
    __mmask16 mask = 0x0001;
    __m512i r = _mm512_mask_expandloadu_epi32(src, mask, buf + 15);
    int* out = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++)
        out[i] = 0xABABABAB;
    _mm512_storeu_si512((__m512i*)out, r);
    ZASSERT(out[0] == buf[15]);
    for (int i = 1; i < 16; i++)
        ZASSERT(out[i] == 0xABABABAB);

    zprintf("expand low mask near upper test passed!\n");
    return 0;
}
