#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 32);
    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);
    __mmask8 mask = 0x0F;
    _mm256_mask_compressstoreu_epi64(buf + 4, mask, v);
    zprintf("Should have failed!\n");
    return 0;
}
