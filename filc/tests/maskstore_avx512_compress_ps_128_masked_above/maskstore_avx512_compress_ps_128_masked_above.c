#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 4; i++) buf[i] = 0;
    __m128 v = _mm_setr_ps(0.0f, 1.0f, 2.0f, 3.0f);
    __mmask8 mask = 0x03;
    _mm_mask_compressstoreu_ps(buf + 2, mask, v);
    ZASSERT(buf[2] == 0);
    ZASSERT(buf[3] == 1);
    for (int i = 0; i < 2; i++)
        ZASSERT(buf[i] == 0);

    zprintf("compressstore masked above test passed!\n");
    return 0;
}
