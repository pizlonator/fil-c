#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    __m512i v = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);
    __mmask8 mask = 0xFF;

    /* Full mask, pointer 8 bytes below lower bound. */
    _mm512_mask_cvtepi64_storeu_epi8(buf - 8, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
