#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    __m128i v = _mm_set_epi64x(1, 0);
    __mmask8 mask = 0x03;

    /* Full mask, pointer 8 bytes below lower bound. */
    _mm_mask_cvtsepi64_storeu_epi32(buf - 8, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
