#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 16);
    for (int i = 0; i < 16; i++) buf[i] = 0xAB;

    __m512i v = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __mmask16 mask = 0;

    /* Zero mask with an OOB pointer must not access memory. */
    _mm512_mask_cvtsepi32_storeu_epi8(buf + 64, mask, v);

    for (int i = 0; i < 16; i++)
        ZASSERT(((unsigned char*)buf)[i] == 0xAB);

    zprintf("maskstore_avx512_pmovs_db_512 zero mask oob test passed!\n");
    return 0;
}
