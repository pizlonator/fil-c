#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);
    __mmask8 mask = 0x0F;

    /* Full mask, pointer 8 bytes below lower bound. */
    _mm256_mask_cvtusepi64_storeu_epi16(buf - 8, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
