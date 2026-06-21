#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512f"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 64);
    __m512 v = _mm512_setr_ps(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f);
    __mmask16 mask = 0xFFFF;
    _mm512_mask_compressstoreu_ps(buf - 16, mask, v);
    zprintf("Should have failed!\n");
    return 0;
}
