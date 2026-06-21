#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 32; i++) buf[i] = 0xAA;

    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __mmask16 mask = 0x00FF;

    /* Low 8 elements active at the top of the allocation. */
    _mm512_mask_cvtepi32_storeu_epi16(((unsigned short*)buf) + 8, mask, v);

    ZASSERT(*((unsigned short*)(buf + 32 - 16 + 0)) == (unsigned short)0);
    ZASSERT(*((unsigned short*)(buf + 32 - 16 + 2)) == (unsigned short)1);
    ZASSERT(*((unsigned short*)(buf + 32 - 16 + 4)) == (unsigned short)2);
    ZASSERT(*((unsigned short*)(buf + 32 - 16 + 6)) == (unsigned short)3);
    ZASSERT(*((unsigned short*)(buf + 32 - 16 + 8)) == (unsigned short)4);
    ZASSERT(*((unsigned short*)(buf + 32 - 16 + 10)) == (unsigned short)5);
    ZASSERT(*((unsigned short*)(buf + 32 - 16 + 12)) == (unsigned short)6);
    ZASSERT(*((unsigned short*)(buf + 32 - 16 + 14)) == (unsigned short)7);
    for (int i = 0; i < 32 - 16; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_dw_512 masked above test passed!\n");
    return 0;
}
