#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512bw,avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m256i v = _mm256_set_epi16(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    __mmask16 mask = 0xFF00;

    /* High 8 elements active, starting just below the lower bound. */
    _mm256_mask_cvtsepi16_storeu_epi8(buf - 8, mask, v);

    ZASSERT(((unsigned char*)buf)[0] == (unsigned char)8);
    ZASSERT(((unsigned char*)buf)[1] == (unsigned char)9);
    ZASSERT(((unsigned char*)buf)[2] == (unsigned char)10);
    ZASSERT(((unsigned char*)buf)[3] == (unsigned char)11);
    ZASSERT(((unsigned char*)buf)[4] == (unsigned char)12);
    ZASSERT(((unsigned char*)buf)[5] == (unsigned char)13);
    ZASSERT(((unsigned char*)buf)[6] == (unsigned char)14);
    ZASSERT(((unsigned char*)buf)[7] == (unsigned char)15);

    zprintf("pmov_wb_256 masked below test passed!\n");
    return 0;
}
