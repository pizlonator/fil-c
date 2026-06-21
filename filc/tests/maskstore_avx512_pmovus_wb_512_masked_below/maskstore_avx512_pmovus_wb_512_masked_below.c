#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m512i v = _mm512_set_epi16(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    __mmask32 mask = 0xFFFF0000;

    /* High 16 elements active, starting just below the lower bound. */
    _mm512_mask_cvtusepi16_storeu_epi8(buf - 16, mask, v);

    ZASSERT(((unsigned char*)buf)[0] == (unsigned char)16);
    ZASSERT(((unsigned char*)buf)[1] == (unsigned char)17);
    ZASSERT(((unsigned char*)buf)[2] == (unsigned char)18);
    ZASSERT(((unsigned char*)buf)[3] == (unsigned char)19);
    ZASSERT(((unsigned char*)buf)[4] == (unsigned char)20);
    ZASSERT(((unsigned char*)buf)[5] == (unsigned char)21);
    ZASSERT(((unsigned char*)buf)[6] == (unsigned char)22);
    ZASSERT(((unsigned char*)buf)[7] == (unsigned char)23);
    ZASSERT(((unsigned char*)buf)[8] == (unsigned char)24);
    ZASSERT(((unsigned char*)buf)[9] == (unsigned char)25);
    ZASSERT(((unsigned char*)buf)[10] == (unsigned char)26);
    ZASSERT(((unsigned char*)buf)[11] == (unsigned char)27);
    ZASSERT(((unsigned char*)buf)[12] == (unsigned char)28);
    ZASSERT(((unsigned char*)buf)[13] == (unsigned char)29);
    ZASSERT(((unsigned char*)buf)[14] == (unsigned char)30);
    ZASSERT(((unsigned char*)buf)[15] == (unsigned char)31);

    zprintf("pmov_wb_512 masked below test passed!\n");
    return 0;
}
