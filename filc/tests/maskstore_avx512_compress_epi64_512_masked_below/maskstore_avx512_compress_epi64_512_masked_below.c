#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 8; i++) buf[i] = 0;
    __m512i v = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);
    __mmask8 mask = 0xF0;
    _mm512_mask_compressstoreu_epi64(buf, mask, v);
    ZASSERT(buf[0] == 4);
    ZASSERT(buf[1] == 5);
    ZASSERT(buf[2] == 6);
    ZASSERT(buf[3] == 7);
    for (int i = 4; i < 8; i++)
        ZASSERT(buf[i] == 0);

    zprintf("compressstore masked below test passed!\n");
    return 0;
}
