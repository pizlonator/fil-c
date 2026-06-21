#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw,avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAA;

    __m256i v = _mm256_set_epi16(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    __mmask16 mask = 0x00FF;

    /* Low 8 elements active at the top of the allocation. */
    _mm256_mask_cvtepi16_storeu_epi8(((unsigned char*)buf) + 8, mask, v);

    ZASSERT(((unsigned char*)buf)[16 - 8 + 0] == (unsigned char)0);
    ZASSERT(((unsigned char*)buf)[16 - 8 + 1] == (unsigned char)1);
    ZASSERT(((unsigned char*)buf)[16 - 8 + 2] == (unsigned char)2);
    ZASSERT(((unsigned char*)buf)[16 - 8 + 3] == (unsigned char)3);
    ZASSERT(((unsigned char*)buf)[16 - 8 + 4] == (unsigned char)4);
    ZASSERT(((unsigned char*)buf)[16 - 8 + 5] == (unsigned char)5);
    ZASSERT(((unsigned char*)buf)[16 - 8 + 6] == (unsigned char)6);
    ZASSERT(((unsigned char*)buf)[16 - 8 + 7] == (unsigned char)7);
    for (int i = 0; i < 16 - 8; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_wb_256 masked above test passed!\n");
    return 0;
}
