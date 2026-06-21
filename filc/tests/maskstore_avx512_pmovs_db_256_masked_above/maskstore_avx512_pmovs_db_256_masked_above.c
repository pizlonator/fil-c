#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAA;

    __m256i v = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0x0F;

    /* Low 4 elements active at the top of the allocation. */
    _mm256_mask_cvtsepi32_storeu_epi8(((unsigned char*)buf) + 12, mask, v);

    ZASSERT(((unsigned char*)buf)[16 - 4 + 0] == (unsigned char)0);
    ZASSERT(((unsigned char*)buf)[16 - 4 + 1] == (unsigned char)1);
    ZASSERT(((unsigned char*)buf)[16 - 4 + 2] == (unsigned char)2);
    ZASSERT(((unsigned char*)buf)[16 - 4 + 3] == (unsigned char)3);
    for (int i = 0; i < 16 - 4; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_db_256 masked above test passed!\n");
    return 0;
}
