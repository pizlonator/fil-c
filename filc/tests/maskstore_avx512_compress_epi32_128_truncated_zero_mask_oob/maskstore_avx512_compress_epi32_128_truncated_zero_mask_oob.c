#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    int* buf = (int*)zgc_aligned_alloc(64, 4 * sizeof(int));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 4 * sizeof(int); i++)
        bytes[i] = 0xAB;
    __m128i v = _mm_setr_epi32(0, 1, 2, 3);
    __mmask8 mask = 0xF0;
    _mm_mask_compressstoreu_epi32(buf + 16, mask, v);
    for (size_t i = 0; i < 4 * sizeof(int); i++)
        ZASSERT(bytes[i] == 0xAB);
    zprintf("compress epi32 128 truncated zero mask oob test passed!\n");
    return 0;
}
