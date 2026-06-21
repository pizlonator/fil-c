#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAA;

    __m128i v = _mm_set_epi64x(1, 0);
    __mmask8 mask = 0x01;

    /* Low 1 elements active at the top of the allocation. */
    _mm_mask_cvtsepi64_storeu_epi8(((unsigned char*)buf) + 15, mask, v);

    ZASSERT(((unsigned char*)buf)[16 - 1 + 0] == (unsigned char)0);
    for (int i = 0; i < 16 - 1; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_qb_128 masked above test passed!\n");
    return 0;
}
