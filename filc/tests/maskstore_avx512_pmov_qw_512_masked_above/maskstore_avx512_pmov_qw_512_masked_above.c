#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAA;

    __m512i v = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);
    __mmask8 mask = 0x0F;

    /* Low 4 elements active at the top of the allocation. */
    _mm512_mask_cvtepi64_storeu_epi16(((unsigned short*)buf) + 4, mask, v);

    ZASSERT(*((unsigned short*)(buf + 16 - 8 + 0)) == (unsigned short)0);
    ZASSERT(*((unsigned short*)(buf + 16 - 8 + 2)) == (unsigned short)1);
    ZASSERT(*((unsigned short*)(buf + 16 - 8 + 4)) == (unsigned short)2);
    ZASSERT(*((unsigned short*)(buf + 16 - 8 + 6)) == (unsigned short)3);
    for (int i = 0; i < 16 - 8; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_qw_512 masked above test passed!\n");
    return 0;
}
