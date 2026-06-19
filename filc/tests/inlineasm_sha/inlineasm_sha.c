#include <stdfil.h>
#include <immintrin.h>

int main(void)
{
    /* SHA1MSG1: intermediate message schedule step. */
    {
        __m128i a = _mm_setr_epi32(0x11111111, 0x22222222, 0x33333333, 0x44444444);
        __m128i b = _mm_setr_epi32(0x55555555, 0x66666666, 0x77777777, 0x88888888);
        __m128i expected = _mm_sha1msg1_epu32(a, b);
        asm volatile("sha1msg1 %1, %0" : "+x"(a) : "x"(b));
        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(a, expected));
        ZASSERT(mask == 0xffff);
    }

    /* SHA1MSG2: final message schedule step. */
    {
        __m128i a = _mm_setr_epi32(0x11111111, 0x22222222, 0x33333333, 0x44444444);
        __m128i b = _mm_setr_epi32(0x55555555, 0x66666666, 0x77777777, 0x88888888);
        __m128i expected = _mm_sha1msg2_epu32(a, b);
        asm volatile("sha1msg2 %1, %0" : "+x"(a) : "x"(b));
        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(a, expected));
        ZASSERT(mask == 0xffff);
    }

    /* SHA1NEXTE: calculate SHA1 state variable E. */
    {
        __m128i a = _mm_setr_epi32(0x11111111, 0x22222222, 0x33333333, 0x44444444);
        __m128i b = _mm_setr_epi32(0x55555555, 0x66666666, 0x77777777, 0x88888888);
        __m128i expected = _mm_sha1nexte_epu32(a, b);
        asm volatile("sha1nexte %1, %0" : "+x"(a) : "x"(b));
        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(a, expected));
        ZASSERT(mask == 0xffff);
    }

    /* SHA1RNDS4: four rounds of SHA1, with an imm8 selector. */
    {
        __m128i a = _mm_setr_epi32(0x11111111, 0x22222222, 0x33333333, 0x44444444);
        __m128i b = _mm_setr_epi32(0x55555555, 0x66666666, 0x77777777, 0x88888888);
        __m128i expected = _mm_sha1rnds4_epu32(a, b, 0);
        asm volatile("sha1rnds4 $0, %1, %0" : "+x"(a) : "x"(b));
        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(a, expected));
        ZASSERT(mask == 0xffff);
    }

    /* SHA256MSG1: intermediate message schedule step. */
    {
        __m128i a = _mm_setr_epi32(0x11111111, 0x22222222, 0x33333333, 0x44444444);
        __m128i b = _mm_setr_epi32(0x55555555, 0x66666666, 0x77777777, 0x88888888);
        __m128i expected = _mm_sha256msg1_epu32(a, b);
        asm volatile("sha256msg1 %1, %0" : "+x"(a) : "x"(b));
        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(a, expected));
        ZASSERT(mask == 0xffff);
    }

    /* SHA256MSG2: final message schedule step. */
    {
        __m128i a = _mm_setr_epi32(0x11111111, 0x22222222, 0x33333333, 0x44444444);
        __m128i b = _mm_setr_epi32(0x55555555, 0x66666666, 0x77777777, 0x88888888);
        __m128i expected = _mm_sha256msg2_epu32(a, b);
        asm volatile("sha256msg2 %1, %0" : "+x"(a) : "x"(b));
        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(a, expected));
        ZASSERT(mask == 0xffff);
    }

    /* SHA256RNDS2: two rounds of SHA256. Reads XMM0 implicitly as an input,
     * so we pin the message-schedule value to xmm0 via a register variable. */
    {
        __m128i a = _mm_setr_epi32(0x11111111, 0x22222222, 0x33333333, 0x44444444);
        __m128i b = _mm_setr_epi32(0x55555555, 0x66666666, 0x77777777, 0x88888888);
        __m128i w = _mm_setr_epi32(0x99999999, 0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc);
        __m128i expected = _mm_sha256rnds2_epu32(a, b, w);

        register __m128i w_reg asm("xmm0") = w;
        asm volatile("sha256rnds2 %1, %0" : "+x"(a) : "x"(b), "x"(w_reg));

        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(a, expected));
        ZASSERT(mask == 0xffff);
    }

    return 0;
}
