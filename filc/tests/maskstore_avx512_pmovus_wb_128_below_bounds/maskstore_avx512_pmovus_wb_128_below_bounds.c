#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw,avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    __m128i v = _mm_set_epi16(7, 6, 5, 4, 3, 2, 1, 0);
    __mmask8 mask = 0xFF;

    /* Full mask, pointer 8 bytes below lower bound. */
    _mm_mask_cvtusepi16_storeu_epi8(buf - 8, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
