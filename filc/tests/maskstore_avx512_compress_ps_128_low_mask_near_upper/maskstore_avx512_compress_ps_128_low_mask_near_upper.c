#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("avx512vl"))) int main()
{
    float* buf = zgc_aligned_alloc(64, 4 * sizeof(float));
    unsigned char* bytes = (unsigned char*)buf;
    for (size_t i = 0; i < 4 * sizeof(float); i++)
        bytes[i] = 0xAB;
    __m128 v = _mm_setr_ps(0, 1, 2, 3);
    __mmask8 mask = 0x03; /* only low elements active */
    _mm_mask_compressstoreu_ps(buf + 2, mask, v);
    for (size_t i = 0; i < 2 * sizeof(float); i++)
        ZASSERT(bytes[i] == 0xAB);
    ZASSERT(buf[2] == 0);
    ZASSERT(buf[3] == 1);
    zprintf("compress ps 128 low mask near upper test passed!\n");
    return 0;
}
