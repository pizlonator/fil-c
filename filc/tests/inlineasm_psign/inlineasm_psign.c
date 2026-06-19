#include <stdfil.h>

typedef signed char v16qi __attribute__((vector_size(16)));
typedef short v8hi __attribute__((vector_size(16)));
typedef int v4si __attribute__((vector_size(16)));

/* PSIGNB/PSIGNW/PSIGND (SSSE3): for each element, negate dest if src < 0,
   zero dest if src == 0, preserve dest if src > 0. */

int main(void)
{
    /* PSIGNB */
    {
        v16qi a = { 5, 5, 5, 5, -5, -5, -5, -5,
                    5, 5, 5, 5, 5, 5, 5, 5 };
        v16qi b = { 1, -1, 0, 2, 1, -1, 0, 2,
                    1, 1, 1, 1, 1, 1, 1, 1 };
        asm volatile("psignb %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 5);
        ZASSERT(a[1] == -5);
        ZASSERT(a[2] == 0);
        ZASSERT(a[3] == 5);
        ZASSERT(a[4] == -5);
        ZASSERT(a[5] == 5);
        ZASSERT(a[6] == 0);
        ZASSERT(a[7] == -5);
        ZASSERT(a[8] == 5);
        ZASSERT(a[15] == 5);
    }

    /* PSIGNW */
    {
        v8hi a = { 100, 100, 100, 100, -100, -100, -100, -100 };
        v8hi b = { 1, -1, 0, 2, 1, -1, 0, 2 };
        asm volatile("psignw %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 100);
        ZASSERT(a[1] == -100);
        ZASSERT(a[2] == 0);
        ZASSERT(a[3] == 100);
        ZASSERT(a[4] == -100);
        ZASSERT(a[5] == 100);
        ZASSERT(a[6] == 0);
        ZASSERT(a[7] == -100);
    }

    /* PSIGND */
    {
        v4si a = { 1000, 1000, 1000, 1000 };
        v4si b = { 1, -1, 0, 2 };
        asm volatile("psignd %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 1000);
        ZASSERT(a[1] == -1000);
        ZASSERT(a[2] == 0);
        ZASSERT(a[3] == 1000);
    }

    return 0;
}
