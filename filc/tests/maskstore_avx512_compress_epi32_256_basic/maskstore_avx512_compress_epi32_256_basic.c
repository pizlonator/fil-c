#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 8; i++) buf[i] = 0;
    __m256i v = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask;
    __mmask8 odd_mask = 0xAA;

    /* Partial odd mask: active lanes are odd-numbered positions. */
    mask = odd_mask;
    _mm256_mask_compressstoreu_epi32(buf, mask, v);
    ZASSERT(buf[0] == 1);
    ZASSERT(buf[1] == 3);
    ZASSERT(buf[2] == 5);
    ZASSERT(buf[3] == 7);
    for (int i = 4; i < 8; i++)
        ZASSERT(buf[i] == 0);

    for (int i = 0; i < 8; i++) buf[i] = 0;
    mask = 0xFF;
    _mm256_mask_compressstoreu_epi32(buf, mask, v);
    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i] == i);

    for (int i = 0; i < 8; i++) buf[i] = 42;
    mask = 0;
    _mm256_mask_compressstoreu_epi32(buf, mask, v);
    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i] == 42);

    zprintf("compressstore basic test passed!\n");
    return 0;
}
