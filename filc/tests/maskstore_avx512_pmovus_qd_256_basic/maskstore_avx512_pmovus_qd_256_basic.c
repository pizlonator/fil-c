#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    char* buf = zgc_aligned_alloc(64, 64);
    for (int i = 0; i < 64; i++) buf[i] = 0;

    __m256i v = _mm256_set_epi64x(3, 2, 1, 0);

    /* Full mask */
    __mmask8 full = 0x0F;
    _mm256_mask_cvtusepi64_storeu_epi32(buf, full, v);
    for (int i = 0; i < 4; i++)
        ZASSERT(((unsigned int*)buf)[i] == (unsigned int)i);

    /* Partial mask (odd elements) */
    for (int i = 0; i < 64; i++) buf[i] = 0;
    __mmask8 partial = 0x0A;
    _mm256_mask_cvtusepi64_storeu_epi32(buf, partial, v);
    for (int i = 0; i < 4; i++) {
            if (i % 2 == 1)
                ZASSERT(((unsigned int*)buf)[i] == (unsigned int)i);
            else
                ZASSERT(((unsigned int*)buf)[i] == 0);
        }

    /* Zero mask - no access */
    for (int i = 0; i < 64; i++) buf[i] = 42;
    __mmask8 zero = 0;
    _mm256_mask_cvtusepi64_storeu_epi32(buf, zero, v);
    for (int i = 0; i < 64; i++)
        ZASSERT(((unsigned char*)buf)[i] == 42);

    zprintf("pmov_qd_256 basic test passed!\n");
    return 0;
}
