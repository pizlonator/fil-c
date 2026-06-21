#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    __m256i v = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0xFF;

    /* Full mask, pointer 16 bytes below lower bound. */
    _mm256_mask_cvtepi32_storeu_epi16(buf - 16, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
