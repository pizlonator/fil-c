#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    double* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 2; i++) buf[i] = 0;
    __m128d v = _mm_setr_pd(0.0, 1.0);
    __mmask8 mask = 0x01;
    _mm_mask_compressstoreu_pd(buf + 1, mask, v);
    ZASSERT(buf[1] == 0);
    for (int i = 0; i < 1; i++)
        ZASSERT(buf[i] == 0);

    zprintf("compressstore masked above test passed!\n");
    return 0;
}
