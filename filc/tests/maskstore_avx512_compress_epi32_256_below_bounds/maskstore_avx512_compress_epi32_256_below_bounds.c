#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f,avx512vl"))) int main()
{
    int* buf = zgc_aligned_alloc(64, 32);
    __m256i v = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0xFF;
    _mm256_mask_compressstoreu_epi32(buf - 8, mask, v);
    zprintf("Should have failed!\n");
    return 0;
}
