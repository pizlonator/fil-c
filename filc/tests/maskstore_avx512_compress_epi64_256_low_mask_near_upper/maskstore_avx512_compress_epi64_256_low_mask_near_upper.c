#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 4 * sizeof(long long));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 4 * sizeof(long long); i++)
        bytes[i] = 0xAB;
    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);
    __mmask8 mask = 0x03; /* only low elements active */
    _mm256_mask_compressstoreu_epi64(buf + 2, mask, v);
    for (size_t i = 0; i < 2 * sizeof(long long); i++)
        ZASSERT(bytes[i] == 0xAB);
    ZASSERT(buf[2] == 0);
    ZASSERT(buf[3] == 1);
    zprintf("compress epi64 256 low mask near upper test passed!\n");
    return 0;
}
