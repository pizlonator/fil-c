#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 8 * sizeof(float));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 8 * sizeof(float); i++)
        bytes[i] = 0xAB;
    __m256 v = _mm256_setr_ps(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0x0F; /* only low elements active */
    _mm256_mask_compressstoreu_ps(buf + 4, mask, v);
    for (size_t i = 0; i < 4 * sizeof(float); i++)
        ZASSERT(bytes[i] == 0xAB);
    ZASSERT(buf[4] == 0);
    ZASSERT(buf[5] == 1);
    ZASSERT(buf[6] == 2);
    ZASSERT(buf[7] == 3);
    zprintf("compress ps 256 low mask near upper test passed!\n");
    return 0;
}
