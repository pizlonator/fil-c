#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 8 * sizeof(long long));
    for (int i = 0; i < 8; i++)
        buf[i] = 0xABABABAB;
    __m512i src = _mm512_set1_epi64(-1);
    __mmask8 mask = 0x0F; /* only low elements active */
    __m512i r = _mm512_mask_expandloadu_epi64(src, mask, buf + 4);
    long long* out = zgc_aligned_alloc(64, 8 * sizeof(long long));
    _mm512_storeu_si512((__m512i*)out, r);
    ZASSERT(out[0] == 0xABABABAB);
    ZASSERT(out[1] == 0xABABABAB);
    ZASSERT(out[2] == 0xABABABAB);
    ZASSERT(out[3] == 0xABABABAB);
    ZASSERT(out[4] == -1);
    ZASSERT(out[5] == -1);
    ZASSERT(out[6] == -1);
    ZASSERT(out[7] == -1);
    zprintf("expand epi64 512 low mask near upper test passed!\n");
    return 0;
}
