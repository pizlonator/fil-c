#include <stdfil.h>

typedef double v2df __attribute__((vector_size(16)));
typedef float v4sf __attribute__((vector_size(16)));

/* SHUFPD (SSE2): packed interleave shuffle of pairs of doubles.
   Intel: SHUFPD xmm1, xmm2/m128, imm8.
   AT&T:  shufpd $imm8, %xmm2, %xmm1
   dst[0] = (imm8 bit0) ? src1[1] : src1[0]   (src1 == xmm1, read+written)
   dst[1] = (imm8 bit1) ? src2[1] : src2[0]   (src2 == xmm2, read) */
__attribute__((target("sse2")))
static void test_shufpd(void)
{
    v2df a = { 1.0, 2.0 };
    v2df b = { 3.0, 4.0 };

    /* imm8 = 0x1: bit0=1, bit1=0 -> { src1[1], src2[0] } = { 2.0, 3.0 } */
    {
        v2df d = a;
        asm volatile("shufpd $0x1, %1, %0" : "+x"(d) : "x"(b));
        ZASSERT(d[0] == 2.0);
        ZASSERT(d[1] == 3.0);
    }

    /* imm8 = 0x2: bit0=0, bit1=1 -> { src1[0], src2[1] } = { 1.0, 4.0 } */
    {
        v2df d = a;
        asm volatile("shufpd $0x2, %1, %0" : "+x"(d) : "x"(b));
        ZASSERT(d[0] == 1.0);
        ZASSERT(d[1] == 4.0);
    }

    /* imm8 = 0x3: both bits set -> { src1[1], src2[1] } = { 2.0, 4.0 } */
    {
        v2df d = a;
        asm volatile("shufpd $0x3, %1, %0" : "+x"(d) : "x"(b));
        ZASSERT(d[0] == 2.0);
        ZASSERT(d[1] == 4.0);
    }
}

/* SHUFPS (SSE): packed interleave shuffle of quadruplets of floats.
   Intel: SHUFPS xmm1, xmm2/m128, imm8.
   AT&T:  shufps $imm8, %xmm2, %xmm1
   dst[0] = src1[imm8 & 3]
   dst[1] = src1[(imm8 >> 2) & 3]
   dst[2] = src2[(imm8 >> 4) & 3]
   dst[3] = src2[(imm8 >> 6) & 3] */
__attribute__((target("sse")))
static void test_shufps(void)
{
    v4sf a = { 1.0f, 2.0f, 3.0f, 4.0f };
    v4sf b = { 5.0f, 6.0f, 7.0f, 8.0f };

    /* imm8 = 0x1b = 0b00_01_10_11 reverses within each half:
       dst = { src1[3], src1[2], src2[1], src2[0] } = { 4, 3, 6, 5 } */
    {
        v4sf d = a;
        asm volatile("shufps $0x1b, %1, %0" : "+x"(d) : "x"(b));
        ZASSERT(d[0] == 4.0f);
        ZASSERT(d[1] == 3.0f);
        ZASSERT(d[2] == 6.0f);
        ZASSERT(d[3] == 5.0f);
    }

    /* imm8 = 0x00: dst = { src1[0], src1[0], src2[0], src2[0] } = {1,1,5,5} */
    {
        v4sf d = a;
        asm volatile("shufps $0x0, %1, %0" : "+x"(d) : "x"(b));
        ZASSERT(d[0] == 1.0f);
        ZASSERT(d[1] == 1.0f);
        ZASSERT(d[2] == 5.0f);
        ZASSERT(d[3] == 5.0f);
    }
}

/* VSHUFPD (AVX): three-source form with separate destination.
   Intel: VSHUFPD xmm1, xmm2, xmm3/m128, imm8.
   AT&T:  vshufpd $imm8, %xmm3, %xmm2, %xmm1
   dst[0] = (imm8 bit0) ? src2[1] : src2[0]   (src2 == first source, xmm2)
   dst[1] = (imm8 bit1) ? src3[1] : src3[0]   (src3 == second source, xmm3) */
__attribute__((target("avx")))
static void test_vshufpd(void)
{
    v2df a = { 1.0, 2.0 };
    v2df b = { 3.0, 4.0 };

    /* imm8 = 0x1 -> { src2[1], src3[0] } = { 2.0, 3.0 } */
    {
        v2df d;
        asm volatile("vshufpd $0x1, %2, %1, %0" : "=x"(d) : "x"(a), "x"(b));
        ZASSERT(d[0] == 2.0);
        ZASSERT(d[1] == 3.0);
    }

    /* imm8 = 0x2 -> { src2[0], src3[1] } = { 1.0, 4.0 } */
    {
        v2df d;
        asm volatile("vshufpd $0x2, %2, %1, %0" : "=x"(d) : "x"(a), "x"(b));
        ZASSERT(d[0] == 1.0);
        ZASSERT(d[1] == 4.0);
    }
}

/* VSHUFPS (AVX): three-source form with separate destination.
   Intel: VSHUFPS xmm1, xmm2, xmm3/m128, imm8.
   AT&T:  vshufps $imm8, %xmm3, %xmm2, %xmm1
   dst[0] = src2[imm8 & 3]
   dst[1] = src2[(imm8 >> 2) & 3]
   dst[2] = src3[(imm8 >> 4) & 3]
   dst[3] = src3[(imm8 >> 6) & 3] */
__attribute__((target("avx")))
static void test_vshufps(void)
{
    v4sf a = { 1.0f, 2.0f, 3.0f, 4.0f };
    v4sf b = { 5.0f, 6.0f, 7.0f, 8.0f };

    /* imm8 = 0x1b -> { src2[3], src2[2], src3[1], src3[0] } = { 4, 3, 6, 5 } */
    {
        v4sf d;
        asm volatile("vshufps $0x1b, %2, %1, %0" : "=x"(d) : "x"(a), "x"(b));
        ZASSERT(d[0] == 4.0f);
        ZASSERT(d[1] == 3.0f);
        ZASSERT(d[2] == 6.0f);
        ZASSERT(d[3] == 5.0f);
    }
}

int main(void)
{
    test_shufpd();
    test_shufps();
    test_vshufpd();
    test_vshufps();
    return 0;
}
