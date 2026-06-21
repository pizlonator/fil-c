#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m512i v = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);
    __mmask8 mask = 0xF0;

    /* High 4 elements active, starting just below the lower bound. */
    _mm512_mask_cvtsepi64_storeu_epi16(buf - 8, mask, v);

    ZASSERT(*((unsigned short*)(buf + 0)) == (unsigned short)4);
    ZASSERT(*((unsigned short*)(buf + 2)) == (unsigned short)5);
    ZASSERT(*((unsigned short*)(buf + 4)) == (unsigned short)6);
    ZASSERT(*((unsigned short*)(buf + 6)) == (unsigned short)7);

    zprintf("pmov_qw_512 masked below test passed!\n");
    return 0;
}
