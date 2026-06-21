#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m128i v = _mm_setr_epi32(0, 1, 2, 3);
    __mmask8 mask = 0x0C;

    /* High 2 elements active, starting just below the lower bound. */
    _mm_mask_cvtsepi32_storeu_epi8(buf - 2, mask, v);

    ZASSERT(((unsigned char*)buf)[0] == (unsigned char)2);
    ZASSERT(((unsigned char*)buf)[1] == (unsigned char)3);

    zprintf("pmov_db_128 masked below test passed!\n");
    return 0;
}
