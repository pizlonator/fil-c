#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __mmask16 mask = 0xFFFF;

    /* Full mask, pointer at the (exclusive) upper bound. */
    _mm512_mask_cvtsepi32_storeu_epi16(buf + 64, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
