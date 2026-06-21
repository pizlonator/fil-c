#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    __m128i v = _mm_set_epi64x(1, 0);
    __mmask8 mask = 0x03;

    /* Full mask, pointer 4 bytes below lower bound. */
    _mm_mask_cvtepi64_storeu_epi16(buf - 4, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
