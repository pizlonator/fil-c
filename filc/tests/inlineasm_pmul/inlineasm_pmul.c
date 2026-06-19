#include <stdfil.h>

typedef signed char v16qi __attribute__((vector_size(16)));
typedef unsigned char v16uqi __attribute__((vector_size(16)));
typedef short v8hi __attribute__((vector_size(16)));
typedef unsigned short v8uhi __attribute__((vector_size(16)));
typedef int v4si __attribute__((vector_size(16)));
typedef unsigned int v4usi __attribute__((vector_size(16)));
typedef long long v2di __attribute__((vector_size(16)));
typedef unsigned long long v2udi __attribute__((vector_size(16)));

int main(void)
{
    /* PMULDQ: multiply packed signed doubleword integers in even-numbered
       elements (0 and 2), produce packed signed quadword results. */
    {
        v4si a = { 2, 99, 3, 99 };
        v4si b = { 4, 99, 5, 99 };
        asm volatile("pmuldq %1, %0" : "+x"(a) : "x"(b));
        union { v4si s; v2di d; } cv;
        cv.s = a;
        ZASSERT(cv.d[0] == 2 * 4);   /* 8 */
        ZASSERT(cv.d[1] == 3 * 5);   /* 15 */
    }

    /* PMULHRSW: packed signed 16-bit multiply, store rounded high 16 bits.
       Formula: result = (a*b + 0x4000) >> 15 */
    {
        v8hi a = { 200, 300, 1, 1, 1, 1, 1, 1 };
        v8hi b = { 200, 300, 1, 1, 1, 1, 1, 1 };
        asm volatile("pmulhrsw %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == ((200 * 200 + 0x4000) >> 15));  /* 1 */
        ZASSERT(a[1] == ((300 * 300 + 0x4000) >> 15));  /* 3 */
    }

    /* PMULHUW: packed unsigned 16-bit multiply, store high 16 bits. */
    {
        v8uhi a = { 1000, 65535, 1, 1, 1, 1, 1, 1 };
        v8uhi b = { 1000, 65535, 1, 1, 1, 1, 1, 1 };
        asm volatile("pmulhuw %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == (unsigned short)((1000u * 1000u) >> 16));  /* 15 */
        ZASSERT(a[1] == (unsigned short)((65535u * 65535u) >> 16)); /* 65534 */
    }

    /* PMULHW: packed signed 16-bit multiply, store high 16 bits. */
    {
        v8hi a = { 1000, -1000, 1, 1, 1, 1, 1, 1 };
        v8hi b = { 1000, 1000, 1, 1, 1, 1, 1, 1 };
        asm volatile("pmulhw %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == (short)((1000 * 1000) >> 16));   /* 15 */
        ZASSERT(a[1] == (short)((-1000 * 1000) >> 16));  /* -16 */
    }

    /* PMULLD: packed signed 32-bit multiply, store low 32 bits. */
    {
        v4si a = { 3, -3, 1000000, 1 };
        v4si b = { 5, 5, 2000, 1 };
        asm volatile("pmulld %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 15);
        ZASSERT(a[1] == -15);
        ZASSERT(a[2] == 2000000000);
        ZASSERT(a[3] == 1);
    }

    /* PMULLW: packed signed 16-bit multiply, store low 16 bits. */
    {
        v8hi a = { 100, 300, -100, 1, 1, 1, 1, 1 };
        v8hi b = { 200, 300, 200, 1, 1, 1, 1, 1 };
        asm volatile("pmullw %1, %0" : "+x"(a) : "x"(b));
        ZASSERT(a[0] == 20000);
        ZASSERT(a[1] == (short)(300 * 300));          /* 24464 */
        ZASSERT(a[2] == -20000);
    }

    /* PMULUDQ: multiply packed unsigned doubleword integers in even-numbered
       elements (0 and 2), produce packed unsigned quadword results. */
    {
        v4usi a = { 3u, 99u, 5u, 99u };
        v4usi b = { 7u, 99u, 11u, 99u };
        asm volatile("pmuludq %1, %0" : "+x"(a) : "x"(b));
        union { v4usi s; v2udi d; } cv;
        cv.s = a;
        ZASSERT(cv.d[0] == 3u * 7u);   /* 21 */
        ZASSERT(cv.d[1] == 5u * 11u);  /* 55 */
    }

    return 0;
}
