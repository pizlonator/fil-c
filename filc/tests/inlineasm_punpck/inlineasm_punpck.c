#include <stdfil.h>

typedef signed char v16qi __attribute__((vector_size(16)));
typedef unsigned char v16uqi __attribute__((vector_size(16)));
typedef short v8hi __attribute__((vector_size(16)));
typedef unsigned short v8uhi __attribute__((vector_size(16)));
typedef int v4si __attribute__((vector_size(16)));
typedef long long v2di __attribute__((vector_size(16)));

int main(void)
{
    /* PUNPCKHBW: interleave high-order bytes. */
    {
        v16qi a = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
        v16qi b = { 100, 101, 102, 103, 104, 105, 106, 107,
                    108, 109, 110, 111, 112, 113, 114, 115 };
        asm volatile("punpckhbw %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 8);
        ZASSERT(a[1] == 108);
        ZASSERT(a[2] == 9);
        ZASSERT(a[3] == 109);
        ZASSERT(a[14] == 15);
        ZASSERT(a[15] == 115);
    }

    /* PUNPCKLBW: interleave low-order bytes. */
    {
        v16qi a = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
        v16qi b = { 100, 101, 102, 103, 104, 105, 106, 107,
                    108, 109, 110, 111, 112, 113, 114, 115 };
        asm volatile("punpcklbw %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 0);
        ZASSERT(a[1] == 100);
        ZASSERT(a[2] == 1);
        ZASSERT(a[3] == 101);
        ZASSERT(a[14] == 7);
        ZASSERT(a[15] == 107);
    }

    /* PUNPCKHWD: interleave high-order words. */
    {
        v8hi a = { 0, 1, 2, 3, 4, 5, 6, 7 };
        v8hi b = { 100, 101, 102, 103, 104, 105, 106, 107 };
        asm volatile("punpckhwd %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 4);
        ZASSERT(a[1] == 104);
        ZASSERT(a[2] == 5);
        ZASSERT(a[3] == 105);
        ZASSERT(a[6] == 7);
        ZASSERT(a[7] == 107);
    }

    /* PUNPCKLWD: interleave low-order words. */
    {
        v8hi a = { 0, 1, 2, 3, 4, 5, 6, 7 };
        v8hi b = { 100, 101, 102, 103, 104, 105, 106, 107 };
        asm volatile("punpcklwd %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 0);
        ZASSERT(a[1] == 100);
        ZASSERT(a[2] == 1);
        ZASSERT(a[3] == 101);
        ZASSERT(a[6] == 3);
        ZASSERT(a[7] == 103);
    }

    /* PUNPCKHDQ: interleave high-order doublewords. */
    {
        v4si a = { 0, 1, 2, 3 };
        v4si b = { 100, 101, 102, 103 };
        asm volatile("punpckhdq %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 2);
        ZASSERT(a[1] == 102);
        ZASSERT(a[2] == 3);
        ZASSERT(a[3] == 103);
    }

    /* PUNPCKLDQ: interleave low-order doublewords. */
    {
        v4si a = { 0, 1, 2, 3 };
        v4si b = { 100, 101, 102, 103 };
        asm volatile("punpckldq %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 0);
        ZASSERT(a[1] == 100);
        ZASSERT(a[2] == 1);
        ZASSERT(a[3] == 101);
    }

    /* PUNPCKHQDQ: interleave high-order quadwords. */
    {
        v2di a = { 0, 1 };
        v2di b = { 100, 101 };
        asm volatile("punpckhqdq %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 1);
        ZASSERT(a[1] == 101);
    }

    /* PUNPCKLQDQ: interleave low-order quadwords. */
    {
        v2di a = { 0, 1 };
        v2di b = { 100, 101 };
        asm volatile("punpcklqdq %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 0);
        ZASSERT(a[1] == 100);
    }

    return 0;
}
