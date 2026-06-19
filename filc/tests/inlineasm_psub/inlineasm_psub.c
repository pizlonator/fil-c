#include <stdfil.h>

typedef signed char v16qi __attribute__((vector_size(16)));
typedef unsigned char v16uqi __attribute__((vector_size(16)));
typedef short v8hi __attribute__((vector_size(16)));
typedef unsigned short v8uhi __attribute__((vector_size(16)));
typedef int v4si __attribute__((vector_size(16)));
typedef long long v2di __attribute__((vector_size(16)));

int main(void)
{
    /* PSUBB: subtract packed bytes. */
    {
        v16qi a = { 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };
        v16qi b = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
        asm volatile("psubb %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 15);
        ZASSERT(a[1] == 13);
        ZASSERT(a[15] == -15);
    }

    /* PSUBW: subtract packed words. */
    {
        v8hi a = { 800, 700, 600, 500, 400, 300, 200, 100 };
        v8hi b = { 1, 2, 3, 4, 5, 6, 7, 8 };
        asm volatile("psubw %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 799);
        ZASSERT(a[3] == 496);
        ZASSERT(a[7] == 92);
    }

    /* PSUBD: subtract packed doublewords. */
    {
        v4si a = { 4000, 3000, 2000, 1000 };
        v4si b = { 1, 2, 3, 4 };
        asm volatile("psubd %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 3999);
        ZASSERT(a[3] == 996);
    }

    /* PSUBQ: subtract packed quadwords. */
    {
        v2di a = { 2000000000LL, 1000000000LL };
        v2di b = { 1LL, 2LL };
        asm volatile("psubq %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 1999999999LL);
        ZASSERT(a[1] == 999999998LL);
    }

    /* PSUBSB: subtract packed signed bytes with signed saturation. */
    {
        v16qi a = { 100, 100, -100, -100, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
        v16qi b = { 100, 50, -100, -50, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        asm volatile("psubsb %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 0);      /* 100 - 100 = 0 */
        ZASSERT(a[1] == 50);     /* 100 - 50 = 50 */
        ZASSERT(a[2] == 0);      /* -100 - (-100) = 0 */
        ZASSERT(a[3] == -50);    /* -100 - (-50) = -50 */
        ZASSERT(a[4] == 9);
    }

    /* PSUBSW: subtract packed signed words with signed saturation. */
    {
        v8hi a = { 30000, 30000, -30000, -30000, 100, 100, 100, 100 };
        v8hi b = { 30000, 10000, -30000, -10000, 1, 1, 1, 1 };
        asm volatile("psubsw %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 0);       /* 30000 - 30000 = 0 */
        ZASSERT(a[1] == 20000);   /* 30000 - 10000 = 20000 */
        ZASSERT(a[2] == 0);       /* -30000 - (-30000) = 0 */
        ZASSERT(a[3] == -20000);  /* -30000 - (-10000) = -20000 */
        ZASSERT(a[4] == 99);
    }

    /* PSUBUSB: subtract packed unsigned bytes with unsigned saturation. */
    {
        v16uqi a = { 200, 250, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
        v16uqi b = { 100, 100, 200, 50, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        asm volatile("psubusb %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 100);    /* 200 - 100 = 100 */
        ZASSERT(a[1] == 150);    /* 250 - 100 = 150 */
        ZASSERT(a[2] == 0);      /* 10 - 200 saturates down to 0 */
        ZASSERT(a[3] == 0);      /* 10 - 50 saturates down to 0 */
        ZASSERT(a[4] == 9);
    }

    /* PSUBUSW: subtract packed unsigned words with unsigned saturation. */
    {
        v8uhi a = { 60000, 50000, 100, 100, 100, 100, 100, 100 };
        v8uhi b = { 10000, 20000, 60000, 200, 1, 1, 1, 1 };
        asm volatile("psubusw %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 50000);  /* 60000 - 10000 = 50000 */
        ZASSERT(a[1] == 30000);  /* 50000 - 20000 = 30000 */
        ZASSERT(a[2] == 0);      /* 100 - 60000 saturates down to 0 */
        ZASSERT(a[3] == 0);      /* 100 - 200 saturates down to 0 */
        ZASSERT(a[4] == 99);
    }

    return 0;
}
