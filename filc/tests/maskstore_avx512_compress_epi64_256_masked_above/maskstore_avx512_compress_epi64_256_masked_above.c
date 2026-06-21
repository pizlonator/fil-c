#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 4; i++) buf[i] = 0;
    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);
    __mmask8 mask = 0x03;
    _mm256_mask_compressstoreu_epi64(buf + 2, mask, v);
    ZASSERT(buf[2] == 0);
    ZASSERT(buf[3] == 1);
    for (int i = 0; i < 2; i++)
        ZASSERT(buf[i] == 0);

    zprintf("compressstore masked above test passed!\n");
    return 0;
}
