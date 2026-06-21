#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 2 * sizeof(double));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 2 * sizeof(double); i++)
        bytes[i] = 0xAB;
    __m128d v = _mm_setr_pd(0, 1);
    __mmask8 mask = 0x01; /* only low elements active */
    _mm_mask_compressstoreu_pd(buf + 1, mask, v);
    for (size_t i = 0; i < 1 * sizeof(double); i++)
        ZASSERT(bytes[i] == 0xAB);
    ZASSERT(buf[1] == 0);
    zprintf("compress pd 128 low mask near upper test passed!\n");
    return 0;
}
