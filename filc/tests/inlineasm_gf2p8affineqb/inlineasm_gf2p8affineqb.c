#include <stdfil.h>
#include <immintrin.h>

__attribute__((target("gfni,avx2"))) int main(void)
{
    __m128i a = _mm_setr_epi8(
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f);
    __m128i b = _mm_setr_epi8(
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f);
#define IMM 0x55
#define IMM_STR "0x55"

    __m128i expected = _mm_gf2p8affine_epi64_epi8(a, b, IMM);

    // Legacy SSE form: dest is read/write.
    __m128i sse_result = a;
    asm volatile("gf2p8affineqb $" IMM_STR ", %1, %0" : "+x"(sse_result) : "x"(b));

    int sse_mask = _mm_movemask_epi8(_mm_cmpeq_epi8(sse_result, expected));
    ZASSERT(sse_mask == 0xffff);

    // VEX form: non-destructive dest.
    __m128i vex_result;
    asm volatile("vgf2p8affineqb $" IMM_STR ", %2, %1, %0"
                 : "=x"(vex_result)
                 : "x"(a), "x"(b));

    int vex_mask = _mm_movemask_epi8(_mm_cmpeq_epi8(vex_result, expected));
    ZASSERT(vex_mask == 0xffff);

    return 0;
}
