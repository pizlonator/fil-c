#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAA;

    __m128i v = _mm_setr_epi32(0, 1, 2, 3);
    __mmask8 mask = 0x03;

    /* Low 2 elements active at the top of the allocation. */
    _mm_mask_cvtusepi32_storeu_epi8(((unsigned char*)buf) + 14, mask, v);

    ZASSERT(((unsigned char*)buf)[16 - 2 + 0] == (unsigned char)0);
    ZASSERT(((unsigned char*)buf)[16 - 2 + 1] == (unsigned char)1);
    for (int i = 0; i < 16 - 2; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_db_128 masked above test passed!\n");
    return 0;
}
