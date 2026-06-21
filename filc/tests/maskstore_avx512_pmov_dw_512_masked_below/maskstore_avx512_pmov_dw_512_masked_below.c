#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __mmask16 mask = 0xFF00;

    /* High 8 elements active, starting just below the lower bound. */
    _mm512_mask_cvtepi32_storeu_epi16(buf - 16, mask, v);

    ZASSERT(*((unsigned short*)(buf + 0)) == (unsigned short)8);
    ZASSERT(*((unsigned short*)(buf + 2)) == (unsigned short)9);
    ZASSERT(*((unsigned short*)(buf + 4)) == (unsigned short)10);
    ZASSERT(*((unsigned short*)(buf + 6)) == (unsigned short)11);
    ZASSERT(*((unsigned short*)(buf + 8)) == (unsigned short)12);
    ZASSERT(*((unsigned short*)(buf + 10)) == (unsigned short)13);
    ZASSERT(*((unsigned short*)(buf + 12)) == (unsigned short)14);
    ZASSERT(*((unsigned short*)(buf + 14)) == (unsigned short)15);

    zprintf("pmov_dw_512 masked below test passed!\n");
    return 0;
}
