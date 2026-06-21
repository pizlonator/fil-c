#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    __m256i v = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0xFF;

    /* Full mask, pointer at the (exclusive) upper bound. */
    _mm256_mask_cvtsepi32_storeu_epi8(buf + 64, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
