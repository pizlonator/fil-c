#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    float* buf = (float*)zgc_aligned_alloc(64, 4 * sizeof(float));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 4 * sizeof(float); i++)
        bytes[i] = 0xAB;
    __m128 v = _mm_setr_ps(0, 1, 2, 3);
    __mmask8 mask = 0xF0;
    _mm_mask_compressstoreu_ps(buf + 16, mask, v);
    for (size_t i = 0; i < 4 * sizeof(float); i++)
        ZASSERT(bytes[i] == 0xAB);
    zprintf("compress ps 128 truncated zero mask oob test passed!\n");
    return 0;
}
