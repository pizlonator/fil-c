#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 2 * sizeof(long long));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 2 * sizeof(long long); i++)
        bytes[i] = 0xAB;
    __m128i v = _mm_set_epi64x(1, 0);
    __mmask8 mask = 0x01; /* only low elements active */
    _mm_mask_compressstoreu_epi64(buf + 1, mask, v);
    for (size_t i = 0; i < 1 * sizeof(long long); i++)
        ZASSERT(bytes[i] == 0xAB);
    ZASSERT(buf[1] == 0);
    zprintf("compress epi64 128 low mask near upper test passed!\n");
    return 0;
}
