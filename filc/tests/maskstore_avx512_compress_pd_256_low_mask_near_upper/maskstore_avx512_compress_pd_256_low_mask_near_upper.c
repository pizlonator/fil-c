#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 4 * sizeof(double));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 4 * sizeof(double); i++)
        bytes[i] = 0xAB;
    __m256d v = _mm256_setr_pd(0, 1, 2, 3);
    __mmask8 mask = 0x03; /* only low elements active */
    _mm256_mask_compressstoreu_pd(buf + 2, mask, v);
    for (size_t i = 0; i < 2 * sizeof(double); i++)
        ZASSERT(bytes[i] == 0xAB);
    ZASSERT(buf[2] == 0);
    ZASSERT(buf[3] == 1);
    zprintf("compress pd 256 low mask near upper test passed!\n");
    return 0;
}
