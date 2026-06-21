#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 8; i++) buf[i] = 0;
    __m256 v = _mm256_setr_ps(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
    __mmask8 mask = 0x0F;
    _mm256_mask_compressstoreu_ps(buf + 4, mask, v);
    ZASSERT(buf[4] == 0);
    ZASSERT(buf[5] == 1);
    ZASSERT(buf[6] == 2);
    ZASSERT(buf[7] == 3);
    for (int i = 0; i < 4; i++)
        ZASSERT(buf[i] == 0);

    zprintf("compressstore masked above test passed!\n");
    return 0;
}
