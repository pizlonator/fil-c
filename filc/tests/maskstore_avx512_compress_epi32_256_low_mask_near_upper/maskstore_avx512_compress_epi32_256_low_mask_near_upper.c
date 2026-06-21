#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 8 * sizeof(int));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 8 * sizeof(int); i++)
        bytes[i] = 0xAB;
    __m256i v = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0x0F; /* only low elements active */
    _mm256_mask_compressstoreu_epi32(buf + 4, mask, v);
    for (size_t i = 0; i < 4 * sizeof(int); i++)
        ZASSERT(bytes[i] == 0xAB);
    ZASSERT(buf[4] == 0);
    ZASSERT(buf[5] == 1);
    ZASSERT(buf[6] == 2);
    ZASSERT(buf[7] == 3);
    zprintf("compress epi32 256 low mask near upper test passed!\n");
    return 0;
}
