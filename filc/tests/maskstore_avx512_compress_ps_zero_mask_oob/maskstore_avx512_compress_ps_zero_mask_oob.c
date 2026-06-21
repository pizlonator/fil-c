#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 16; i++)
        buf[i] = -1.0f;
    __m512 v = _mm512_setr_ps(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

    /* Zero mask with an OOB pointer must not access memory. */
    __mmask16 mask = 0;
    _mm512_mask_compressstoreu_ps(buf + 16, mask, v);

    for (int i = 0; i < 16; i++)
        ZASSERT(buf[i] == -1.0f);

    zprintf("compress ps zero mask oob test passed!\n");
    return 0;
}
