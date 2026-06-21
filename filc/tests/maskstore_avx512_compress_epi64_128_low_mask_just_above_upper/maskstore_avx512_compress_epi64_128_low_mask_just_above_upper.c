#include <stdfil.h>

#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    long long* buf = zgc_aligned_alloc(64, 2 * sizeof(long long));

    __m128i v = _mm_set_epi64x(1, 0);
    __mmask8 mask = 0x1;

    /* For compressstore the first active element is written at ptr.
       popcount=1 -> one element accessed starting at buf + 2, which is OOB. */
    _mm_mask_compressstoreu_epi64(buf + 2, mask, v);

    zprintf("Should have failed!\n");
    return 0;
}
