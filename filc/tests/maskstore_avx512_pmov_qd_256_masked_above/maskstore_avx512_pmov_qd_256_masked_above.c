#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAA;

    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);
    __mmask8 mask = 0x03;

    /* Low 2 elements active at the top of the allocation. */
    _mm256_mask_cvtepi64_storeu_epi32(((unsigned int*)buf) + 2, mask, v);

    ZASSERT(*((unsigned int*)(buf + 16 - 8 + 0)) == (unsigned int)0);
    ZASSERT(*((unsigned int*)(buf + 16 - 8 + 4)) == (unsigned int)1);
    for (int i = 0; i < 16 - 8; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_qd_256 masked above test passed!\n");
    return 0;
}
