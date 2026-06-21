#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 32; i++) buf[i] = 0xAA;

    __m512i v = _mm512_set_epi16(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    __mmask32 mask = 0x0000FFFF;

    /* Low 16 elements active at the top of the allocation. */
    _mm512_mask_cvtusepi16_storeu_epi8(((unsigned char*)buf) + 16, mask, v);

    ZASSERT(((unsigned char*)buf)[32 - 16 + 0] == (unsigned char)0);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 1] == (unsigned char)1);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 2] == (unsigned char)2);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 3] == (unsigned char)3);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 4] == (unsigned char)4);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 5] == (unsigned char)5);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 6] == (unsigned char)6);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 7] == (unsigned char)7);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 8] == (unsigned char)8);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 9] == (unsigned char)9);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 10] == (unsigned char)10);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 11] == (unsigned char)11);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 12] == (unsigned char)12);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 13] == (unsigned char)13);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 14] == (unsigned char)14);
    ZASSERT(((unsigned char*)buf)[32 - 16 + 15] == (unsigned char)15);
    for (int i = 0; i < 32 - 16; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_wb_512 masked above test passed!\n");
    return 0;
}
