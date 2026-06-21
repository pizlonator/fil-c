#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 8; i++)
        buf[i] = -1;
    __m512i v = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);

    /* Zero mask with an OOB pointer must not access memory. */
    __mmask8 mask = 0;
    _mm512_mask_compressstoreu_epi64(buf + 8, mask, v);

    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i] == -1);

    zprintf("compress epi64 zero mask oob test passed!\n");
    return 0;
}
