#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw,avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    __m256i v = _mm256_set_epi16(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    __mmask16 mask = 0xFFFF;

    /* Full mask, pointer at the (exclusive) upper bound. */
    _mm256_mask_cvtepi16_storeu_epi8(buf + 64, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
