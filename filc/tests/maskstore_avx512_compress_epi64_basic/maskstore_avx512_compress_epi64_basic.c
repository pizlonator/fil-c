#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 8; i++) buf[i] = 0;

    __m512i v = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);

    __mmask8 full = 0xFF;
    _mm512_mask_compressstoreu_epi64(buf, full, v);
    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i] == i);

    for (int i = 0; i < 8; i++) buf[i] = 0;
    __mmask8 partial = 0x0F;
    _mm512_mask_compressstoreu_epi64(buf, partial, v);
    for (int i = 0; i < 4; i++)
        ZASSERT(buf[i] == i);
    for (int i = 4; i < 8; i++)
        ZASSERT(buf[i] == 0);

    for (int i = 0; i < 8; i++) buf[i] = 42;
    __mmask8 zero = 0;
    _mm512_mask_compressstoreu_epi64(buf, zero, v);
    for (int i = 0; i < 8; i++)
        ZASSERT(buf[i] == 42);

    zprintf("compress epi64 basic test passed!\n");
    return 0;
}
