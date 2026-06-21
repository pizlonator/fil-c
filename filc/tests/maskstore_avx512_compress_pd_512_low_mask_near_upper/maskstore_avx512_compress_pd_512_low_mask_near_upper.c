#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 8 * sizeof(double));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 8 * sizeof(double); i++)
        bytes[i] = 0xAB;
    __m512d v = _mm512_setr_pd(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0x0F; /* only low elements active */
    _mm512_mask_compressstoreu_pd(buf + 4, mask, v);
    for (size_t i = 0; i < 4 * sizeof(double); i++)
        ZASSERT(bytes[i] == 0xAB);
    ZASSERT(buf[4] == 0);
    ZASSERT(buf[5] == 1);
    ZASSERT(buf[6] == 2);
    ZASSERT(buf[7] == 3);
    zprintf("compress pd 512 low mask near upper test passed!\n");
    return 0;
}
