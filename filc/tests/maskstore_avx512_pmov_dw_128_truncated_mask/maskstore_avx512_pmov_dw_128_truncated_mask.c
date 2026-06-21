#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0xAA;

    __m128i v = _mm_setr_epi32(0, 1, 2, 3);
    __mmask8 mask = 0xF1;   /* high bits set, only element 0 active */
    int e = 2;
    _mm_mask_cvtepi32_storeu_epi16(buf + 64 - e, mask, v);

    ZASSERT(*((unsigned short*)(buf + 64 - 2)) == 0);
    for (int i = 0; i < 64 - e; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_dw_128 truncated mask test passed!\n");
    return 0;
}
