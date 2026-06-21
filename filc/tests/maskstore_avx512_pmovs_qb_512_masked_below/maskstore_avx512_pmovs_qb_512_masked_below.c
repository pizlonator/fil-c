#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m512i v = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);
    __mmask8 mask = 0xF0;

    /* High 4 elements active, starting just below the lower bound. */
    _mm512_mask_cvtsepi64_storeu_epi8(buf - 4, mask, v);

    ZASSERT(((unsigned char*)buf)[0] == (unsigned char)4);
    ZASSERT(((unsigned char*)buf)[1] == (unsigned char)5);
    ZASSERT(((unsigned char*)buf)[2] == (unsigned char)6);
    ZASSERT(((unsigned char*)buf)[3] == (unsigned char)7);

    zprintf("pmov_qb_512 masked below test passed!\n");
    return 0;
}
