#include <stdfil.h>

typedef float v4sf __attribute__((vector_size(16)));

__attribute__((target("avx"))) int main(void)
{
    /* RSQRTPS: packed single-precision reciprocal square root (SSE). The
     * destination is fully overwritten (all four elements computed from the
     * source), so it is an output-only operand. The result is an approximation
     * with a relative error of at most 1.5 * 2^-12. */
    {
        v4sf a = { 1.0f, 4.0f, 16.0f, 0.25f };
        v4sf b;
        asm volatile("rsqrtps %1, %0" : "=x"(b) : "x"(a));
        /* 1/sqrt(1)=1, 1/sqrt(4)=0.5, 1/sqrt(16)=0.25, 1/sqrt(0.25)=2 */
        ZASSERT(b[0] > 0.999f && b[0] < 1.001f);
        ZASSERT(b[1] > 0.499f && b[1] < 0.501f);
        ZASSERT(b[2] > 0.249f && b[2] < 0.251f);
        ZASSERT(b[3] > 1.999f && b[3] < 2.001f);
    }

    /* RSQRTSS: scalar single-precision reciprocal square root (SSE). Only the
     * low element is replaced; the upper three elements of the destination are
     * preserved, so the destination is a read/write operand. */
    {
        v4sf a = { 4.0f, 7.0f, 8.0f, 9.0f };
        v4sf b = { 16.0f, 0.0f, 0.0f, 0.0f };
        asm volatile("rsqrtss %1, %0" : "+x"(a) : "x"(b));
        /* 1/sqrt(16) = 0.25 */
        ZASSERT(a[0] > 0.249f && a[0] < 0.251f);
        ZASSERT(a[1] == 7.0f);
        ZASSERT(a[2] == 8.0f);
        ZASSERT(a[3] == 9.0f);
    }

    /* VRSQRTPS: packed single-precision reciprocal square root (AVX, 128-bit).
     * Two operands, destination is output-only. */
    {
        v4sf a = { 1.0f, 4.0f, 16.0f, 0.25f };
        v4sf b;
        asm volatile("vrsqrtps %1, %0" : "=x"(b) : "x"(a));
        ZASSERT(b[0] > 0.999f && b[0] < 1.001f);
        ZASSERT(b[1] > 0.499f && b[1] < 0.501f);
        ZASSERT(b[2] > 0.249f && b[2] < 0.251f);
        ZASSERT(b[3] > 1.999f && b[3] < 2.001f);
    }

    /* VRSQRTSS: scalar single-precision reciprocal square root (AVX, 128-bit).
     * Three operands: dest, src1 (upper bits copied), src2 (low element
     * inverted). AT&T ordering is src2, src1, dest. */
    {
        v4sf a = { 0.0f, 7.0f, 8.0f, 9.0f };
        v4sf b = { 16.0f, 0.0f, 0.0f, 0.0f };
        v4sf c;
        asm volatile("vrsqrtss %2, %1, %0" : "=x"(c) : "x"(a), "x"(b));
        /* 1/sqrt(16) = 0.25 */
        ZASSERT(c[0] > 0.249f && c[0] < 0.251f);
        ZASSERT(c[1] == 7.0f);
        ZASSERT(c[2] == 8.0f);
        ZASSERT(c[3] == 9.0f);
    }

    return 0;
}
