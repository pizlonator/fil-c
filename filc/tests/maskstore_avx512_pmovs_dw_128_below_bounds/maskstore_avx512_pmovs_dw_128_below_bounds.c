#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    __m128i v = _mm_setr_epi32(0, 1, 2, 3);
    __mmask8 mask = 0x0F;

    /* Full mask, pointer 8 bytes below lower bound. */
    _mm_mask_cvtsepi32_storeu_epi16(buf - 8, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
