#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAA;

    __m128i v = _mm_set_epi64x(1, 0);
    __mmask8 mask = 0x01;

    /* Low 1 elements active at the top of the allocation. */
    _mm_mask_cvtepi64_storeu_epi16(((unsigned short*)buf) + 7, mask, v);

    ZASSERT(*((unsigned short*)(buf + 16 - 2 + 0)) == (unsigned short)0);
    for (int i = 0; i < 16 - 2; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_qw_128 masked above test passed!\n");
    return 0;
}
