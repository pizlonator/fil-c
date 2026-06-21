#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 8; i++) buf[i] = 0;
    __m256i v = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0xF0;
    _mm256_mask_compressstoreu_epi32(buf, mask, v);
    ZASSERT(buf[0] == 4);
    ZASSERT(buf[1] == 5);
    ZASSERT(buf[2] == 6);
    ZASSERT(buf[3] == 7);
    for (int i = 4; i < 8; i++)
        ZASSERT(buf[i] == 0);

    zprintf("compressstore masked below test passed!\n");
    return 0;
}
