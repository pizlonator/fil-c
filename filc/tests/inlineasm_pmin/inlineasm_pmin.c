#include <stdfil.h>

typedef signed char v16qi __attribute__((vector_size(16)));
typedef unsigned char v16uqi __attribute__((vector_size(16)));
typedef short v8hi __attribute__((vector_size(16)));
typedef unsigned short v8uhi __attribute__((vector_size(16)));
typedef int v4si __attribute__((vector_size(16)));
typedef unsigned int v4usi __attribute__((vector_size(16)));

int main(void)
{
    /* PMINSB: packed minimum of signed bytes. */
    {
        v16qi a = { 1, -5, 100, -100, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        v16qi b = { 3, 2, -10, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        asm volatile("pminsb %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 1);
        ZASSERT(a[1] == -5);
        ZASSERT(a[2] == -10);
        ZASSERT(a[3] == -100);
    }

    /* PMINSW: packed minimum of signed words. */
    {
        v8hi a = { -30000, 1000, 1, 1, 1, 1, 1, 1 };
        v8hi b = { 30000, -1000, 1, 1, 1, 1, 1, 1 };
        asm volatile("pminsw %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == -30000);
        ZASSERT(a[1] == -1000);
    }

    /* PMINSD: packed minimum of signed doublewords. */
    {
        v4si a = { -1000000, 5, 1, 1 };
        v4si b = { 1000000, -5, 1, 1 };
        asm volatile("pminsd %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == -1000000);
        ZASSERT(a[1] == -5);
    }

    /* PMINUB: packed minimum of unsigned bytes. */
    {
        v16uqi a = { 200, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        v16uqi b = { 100, 250, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        asm volatile("pminub %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 100);
        ZASSERT(a[1] == 1);
    }

    /* PMINUW: packed minimum of unsigned words. */
    {
        v8uhi a = { 10, 60000, 1, 1, 1, 1, 1, 1 };
        v8uhi b = { 20, 5, 1, 1, 1, 1, 1, 1 };
        asm volatile("pminuw %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 10);
        ZASSERT(a[1] == 5);
    }

    /* PMINUD: packed minimum of unsigned doublewords. */
    {
        v4usi a = { 1u, 4000000000u, 1u, 1u };
        v4usi b = { 2u, 1u, 1u, 1u };
        asm volatile("pminud %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 1u);
        ZASSERT(a[1] == 1u);
    }

    return 0;
}
