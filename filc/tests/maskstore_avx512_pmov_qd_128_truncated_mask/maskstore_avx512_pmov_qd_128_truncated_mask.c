#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0xAA;

    __m128i v = _mm_set_epi64x(1, 0);
    __mmask8 mask = 0xFD;   /* high bits set, only element 0 active */
    int e = 4;
    _mm_mask_cvtepi64_storeu_epi32(buf + 64 - e, mask, v);

    ZASSERT(*((unsigned int*)(buf + 64 - 4)) == 0);
    for (int i = 0; i < 64 - e; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAA);

    zprintf("pmov_qd_128 truncated mask test passed!\n");
    return 0;
}
