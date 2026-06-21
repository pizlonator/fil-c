#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __mmask16 mask = 0xFF00;

    /* High 8 elements active, starting just below the lower bound. */
    _mm512_mask_cvtsepi32_storeu_epi8(buf - 8, mask, v);

    ZASSERT(((unsigned char*)buf)[0] == (unsigned char)8);
    ZASSERT(((unsigned char*)buf)[1] == (unsigned char)9);
    ZASSERT(((unsigned char*)buf)[2] == (unsigned char)10);
    ZASSERT(((unsigned char*)buf)[3] == (unsigned char)11);
    ZASSERT(((unsigned char*)buf)[4] == (unsigned char)12);
    ZASSERT(((unsigned char*)buf)[5] == (unsigned char)13);
    ZASSERT(((unsigned char*)buf)[6] == (unsigned char)14);
    ZASSERT(((unsigned char*)buf)[7] == (unsigned char)15);

    zprintf("pmov_db_512 masked below test passed!\n");
    return 0;
}
