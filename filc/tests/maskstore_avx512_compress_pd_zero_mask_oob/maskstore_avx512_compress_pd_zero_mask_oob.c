#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 8; i++)
        buf[i] = -1.0;
    __m512d v = _mm512_setr_pd(0, 1, 2, 3, 4, 5, 6, 7);

    /* Zero mask with an OOB pointer must not access memory. */
    __mmask8 mask = 0;
    _mm512_mask_compressstoreu_pd(buf + 8, mask, v);

    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i] == -1.0);

    zprintf("compress pd zero mask oob test passed!\n");
    return 0;
}
