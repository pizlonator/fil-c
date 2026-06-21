#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 16);
    __m128i v = _mm_set_epi64x(1, 0);
    __mmask8 mask = 0x03;
    _mm_mask_compressstoreu_epi64(buf + 2, mask, v);
    zprintf("Should have failed!\n");
    return 0;
}
