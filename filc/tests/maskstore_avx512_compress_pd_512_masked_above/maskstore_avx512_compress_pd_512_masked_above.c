#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 8; i++) buf[i] = 0;
    __m512d v = _mm512_setr_pd(0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0);
    __mmask8 mask = 0x0F;
    _mm512_mask_compressstoreu_pd(buf + 4, mask, v);
    ZASSERT(buf[4] == 0);
    ZASSERT(buf[5] == 1);
    ZASSERT(buf[6] == 2);
    ZASSERT(buf[7] == 3);
    for (int i = 0; i < 4; i++)
        ZASSERT(buf[i] == 0);

    zprintf("compressstore masked above test passed!\n");
    return 0;
}
