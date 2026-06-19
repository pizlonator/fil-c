#include <stdfil.h>

typedef float v4sf __attribute__((vector_size(16)));
typedef double v2df __attribute__((vector_size(16)));

__attribute__((target("avx"))) int main(void)
{
    /* SQRTPS: packed single-precision square root (SSE). The destination is
     * fully overwritten, so it is an output-only operand. */
    {
        v4sf a = { 1.0f, 4.0f, 9.0f, 16.0f };
        v4sf b;
        asm volatile("sqrtps %1, %0" : "=x"(b) : "x"(a));
        ZASSERT(b[0] == 1.0f);
        ZASSERT(b[1] == 2.0f);
        ZASSERT(b[2] == 3.0f);
        ZASSERT(b[3] == 4.0f);
    }

    /* SQRTPD: packed double-precision square root (SSE2). Output-only dest. */
    {
        v2df a = { 4.0, 16.0 };
        v2df b;
        asm volatile("sqrtpd %1, %0" : "=x"(b) : "x"(a));
        ZASSERT(b[0] == 2.0);
        ZASSERT(b[1] == 4.0);
    }

    /* SQRTSS: scalar single-precision square root (SSE). Only the low element
     * is replaced; the upper elements are preserved, so dest is read/write. */
    {
        v4sf a = { 4.0f, 7.0f, 8.0f, 9.0f };
        v4sf b = { 16.0f, 0.0f, 0.0f, 0.0f };
        asm volatile("sqrtss %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 4.0f);
        ZASSERT(a[1] == 7.0f);
        ZASSERT(a[2] == 8.0f);
        ZASSERT(a[3] == 9.0f);
    }

    /* SQRTSD: scalar double-precision square root (SSE2). Dest is read/write. */
    {
        v2df a = { 4.0, 7.0 };
        v2df b = { 16.0, 0.0 };
        asm volatile("sqrtsd %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 4.0);
        ZASSERT(a[1] == 7.0);
    }

    /* VSQRTPS: packed single-precision square root (AVX, 128-bit). */
    {
        v4sf a = { 1.0f, 4.0f, 9.0f, 16.0f };
        v4sf b;
        asm volatile("vsqrtps %1, %0" : "=x"(b) : "x"(a));
        ZASSERT(b[0] == 1.0f);
        ZASSERT(b[1] == 2.0f);
        ZASSERT(b[2] == 3.0f);
        ZASSERT(b[3] == 4.0f);
    }

    /* VSQRTPD: packed double-precision square root (AVX, 128-bit). */
    {
        v2df a = { 4.0, 16.0 };
        v2df b;
        asm volatile("vsqrtpd %1, %0" : "=x"(b) : "x"(a));
        ZASSERT(b[0] == 2.0);
        ZASSERT(b[1] == 4.0);
    }

    /* VSQRTSS: scalar single-precision square root (AVX, 128-bit). Three
     * operands: dest, src1 (upper bits copied), src2 (low element sqrt).
     * AT&T ordering is src2, src1, dest. */
    {
        v4sf a = { 0.0f, 7.0f, 8.0f, 9.0f };
        v4sf b = { 16.0f, 0.0f, 0.0f, 0.0f };
        v4sf c;
        asm volatile("vsqrtss %2, %1, %0" : "=x"(c) : "x"(a), "x"(b));
        ZASSERT(c[0] == 4.0f);
        ZASSERT(c[1] == 7.0f);
        ZASSERT(c[2] == 8.0f);
        ZASSERT(c[3] == 9.0f);
    }

    /* VSQRTSD: scalar double-precision square root (AVX, 128-bit). Three
     * operands. */
    {
        v2df a = { 0.0, 7.0 };
        v2df b = { 16.0, 0.0 };
        v2df c;
        asm volatile("vsqrtsd %2, %1, %0" : "=x"(c) : "x"(a), "x"(b));
        ZASSERT(c[0] == 4.0);
        ZASSERT(c[1] == 7.0);
    }

    return 0;
}
