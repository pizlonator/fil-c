#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    double* buf = (double*)zgc_aligned_alloc(64, 2 * sizeof(double));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 2 * sizeof(double); i++)
        bytes[i] = 0xAB;
    __m128d v = _mm_setr_pd(0, 1);
    __mmask8 mask = 0xF0;
    _mm_mask_compressstoreu_pd(buf + 16, mask, v);
    for (size_t i = 0; i < 2 * sizeof(double); i++)
        ZASSERT(bytes[i] == 0xAB);
    zprintf("compress pd 128 truncated zero mask oob test passed!\n");
    return 0;
}
