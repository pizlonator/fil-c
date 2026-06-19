#include <stdfil.h>

int main(void)
{
    unsigned long a, b, hi, lo;

    /* 0xFFFFFFFF * 2 = 0x1FFFFFFFE fits in 64 bits: hi=0, lo=0x1FFFFFFFE. */
    a = 0xFFFFFFFFUL;
    b = 2UL;
    asm volatile("mulx %2, %0, %1"
                 : "=r"(lo), "=r"(hi)
                 : "r"(b), "d"(a));
    ZASSERT(hi == 0);
    ZASSERT(lo == 0x1FFFFFFFEUL);

    /* 0xFFFFFFFFFFFFFFFF * 0xFFFFFFFFFFFFFFFF
       = 0xFFFFFFFFFFFFFFFE0000000000000001: hi=0xFFFFFFFFFFFFFFFE, lo=1. */
    a = 0xFFFFFFFFFFFFFFFFUL;
    b = 0xFFFFFFFFFFFFFFFFUL;
    asm volatile("mulx %2, %0, %1"
                 : "=r"(lo), "=r"(hi)
                 : "r"(b), "d"(a));
    ZASSERT(hi == 0xFFFFFFFFFFFFFFFEUL);
    ZASSERT(lo == 1);

    /* Small case: 3 * 7 = 21, hi=0, lo=21. */
    a = 3UL;
    b = 7UL;
    asm volatile("mulx %2, %0, %1"
                 : "=r"(lo), "=r"(hi)
                 : "r"(b), "d"(a));
    ZASSERT(hi == 0);
    ZASSERT(lo == 21);

    /* 0x100000000 * 0x100000000 = 0x10000000000000000: hi=1, lo=0. */
    a = 0x100000000UL;
    b = 0x100000000UL;
    asm volatile("mulx %2, %0, %1"
                 : "=r"(lo), "=r"(hi)
                 : "r"(b), "d"(a));
    ZASSERT(hi == 1);
    ZASSERT(lo == 0);

    return 0;
}
