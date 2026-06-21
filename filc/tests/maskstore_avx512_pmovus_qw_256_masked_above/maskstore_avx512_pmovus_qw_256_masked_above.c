#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAA;

    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);
    __mmask8 mask = 0x03;

    /* Low 2 elements active at the top of the allocation. */
    _mm256_mask_cvtusepi64_storeu_epi16(((unsigned short*)buf) + 6, mask, v);

    ZASSERT(*((unsigned short*)(buf + 16 - 4 + 0)) == (unsigned short)0);
    ZASSERT(*((unsigned short*)(buf + 16 - 4 + 2)) == (unsigned short)1);
    for (int i = 0; i < 16 - 4; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_qw_256 masked above test passed!\n");
    return 0;
}
