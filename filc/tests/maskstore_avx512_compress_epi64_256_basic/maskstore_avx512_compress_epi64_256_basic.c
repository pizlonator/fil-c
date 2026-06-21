#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 4; i++) buf[i] = 0;
    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);
    __mmask8 mask;
    __mmask8 odd_mask = 0x0A;

    /* Partial odd mask: active lanes are odd-numbered positions. */
    mask = odd_mask;
    _mm256_mask_compressstoreu_epi64(buf, mask, v);
    ZASSERT(buf[0] == 1);
    ZASSERT(buf[1] == 3);
    for (int i = 2; i < 4; i++)
        ZASSERT(buf[i] == 0);

    for (int i = 0; i < 4; i++) buf[i] = 0;
    mask = 0x0F;
    _mm256_mask_compressstoreu_epi64(buf, mask, v);
    for (int i = 0; i < 4; i++)
        ZASSERT(buf[i] == i);

    for (int i = 0; i < 4; i++) buf[i] = 42;
    mask = 0;
    _mm256_mask_compressstoreu_epi64(buf, mask, v);
    for (int i = 0; i < 4; i++)
        ZASSERT(buf[i] == 42);

    zprintf("compressstore basic test passed!\n");
    return 0;
}
