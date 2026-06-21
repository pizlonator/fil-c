#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 4; i++) buf[i] = 0;
    __m128i v = _mm_setr_epi32(0, 1, 2, 3);
    __mmask8 mask = 0x0C;
    _mm_mask_compressstoreu_epi32(buf, mask, v);
    ZASSERT(buf[0] == 2);
    ZASSERT(buf[1] == 3);
    for (int i = 2; i < 4; i++)
        ZASSERT(buf[i] == 0);

    zprintf("compressstore masked below test passed!\n");
    return 0;
}
