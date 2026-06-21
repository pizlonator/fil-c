#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 2; i++) buf[i] = 0;
    __m128i v = _mm_set_epi64x(1, 0);
    __mmask8 mask = 0x02;
    _mm_mask_compressstoreu_epi64(buf, mask, v);
    ZASSERT(buf[0] == 1);
    for (int i = 1; i < 2; i++)
        ZASSERT(buf[i] == 0);

    zprintf("compressstore masked below test passed!\n");
    return 0;
}
