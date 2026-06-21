#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);
    __mmask8 mask = 0x0F;

    /* Full mask, pointer at the (exclusive) upper bound. */
    _mm256_mask_cvtusepi64_storeu_epi32(buf + 64, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
