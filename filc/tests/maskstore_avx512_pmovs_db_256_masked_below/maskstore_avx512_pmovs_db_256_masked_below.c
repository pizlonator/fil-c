#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m256i v = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    __mmask8 mask = 0xF0;

    /* High 4 elements active, starting just below the lower bound. */
    _mm256_mask_cvtsepi32_storeu_epi8(buf - 4, mask, v);

    ZASSERT(((unsigned char*)buf)[0] == (unsigned char)4);
    ZASSERT(((unsigned char*)buf)[1] == (unsigned char)5);
    ZASSERT(((unsigned char*)buf)[2] == (unsigned char)6);
    ZASSERT(((unsigned char*)buf)[3] == (unsigned char)7);

    zprintf("pmov_db_256 masked below test passed!\n");
    return 0;
}
