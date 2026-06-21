#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    __m128i v = _mm_setr_epi32(0, 1, 2, 3);
    __mmask8 mask = 0x0F;

    /* Full mask, pointer 4 bytes below lower bound. */
    _mm_mask_cvtepi32_storeu_epi8(buf - 4, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
