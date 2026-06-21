#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAA;

    __m256i v = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0x0F;

    /* Low 4 elements active at the top of the allocation. */
    _mm256_mask_cvtusepi32_storeu_epi16(((unsigned short*)buf) + 4, mask, v);

    ZASSERT(*((unsigned short*)(buf + 16 - 8 + 0)) == (unsigned short)0);
    ZASSERT(*((unsigned short*)(buf + 16 - 8 + 2)) == (unsigned short)1);
    ZASSERT(*((unsigned short*)(buf + 16 - 8 + 4)) == (unsigned short)2);
    ZASSERT(*((unsigned short*)(buf + 16 - 8 + 6)) == (unsigned short)3);
    for (int i = 0; i < 16 - 8; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_dw_256 masked above test passed!\n");
    return 0;
}
