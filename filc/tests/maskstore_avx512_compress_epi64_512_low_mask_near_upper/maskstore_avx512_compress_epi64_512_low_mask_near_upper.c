#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 8 * sizeof(long long));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 8 * sizeof(long long); i++)
        bytes[i] = 0xAB;
    __m512i v = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);
    __mmask8 mask = 0x0F; /* only low elements active */
    _mm512_mask_compressstoreu_epi64(buf + 4, mask, v);
    for (size_t i = 0; i < 4 * sizeof(long long); i++)
        ZASSERT(bytes[i] == 0xAB);
    ZASSERT(buf[4] == 0);
    ZASSERT(buf[5] == 1);
    ZASSERT(buf[6] == 2);
    ZASSERT(buf[7] == 3);
    zprintf("compress epi64 512 low mask near upper test passed!\n");
    return 0;
}
