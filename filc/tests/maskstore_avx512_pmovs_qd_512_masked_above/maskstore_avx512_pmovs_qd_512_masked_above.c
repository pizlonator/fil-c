#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 32);
    for (int i = 0; i < 32; i++) buf[i] = 0xAA;

    __m512i v = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);
    __mmask8 mask = 0x0F;

    /* Low 4 elements active at the top of the allocation. */
    _mm512_mask_cvtsepi64_storeu_epi32(((unsigned int*)buf) + 4, mask, v);

    ZASSERT(*((unsigned int*)(buf + 32 - 16 + 0)) == (unsigned int)0);
    ZASSERT(*((unsigned int*)(buf + 32 - 16 + 4)) == (unsigned int)1);
    ZASSERT(*((unsigned int*)(buf + 32 - 16 + 8)) == (unsigned int)2);
    ZASSERT(*((unsigned int*)(buf + 32 - 16 + 12)) == (unsigned int)3);
    for (int i = 0; i < 32 - 16; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_qd_512 masked above test passed!\n");
    return 0;
}
