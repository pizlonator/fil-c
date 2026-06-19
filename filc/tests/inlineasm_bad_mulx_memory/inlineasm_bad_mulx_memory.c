#include <stdfil.h>

int main(void)
{
    unsigned long a = 0xFFFFFFFFUL;
    unsigned long b = 2UL;
    unsigned long hi = 0, lo = 0;
    unsigned long p = 0;
    /* The third operand of mulx may be a memory operand, but memory accesses
       are not supported in safe inline asm. */
    asm volatile("mulx (%3), %0, %1"
                 : "=r"(lo), "=r"(hi)
                 : "d"(a), "r"(p));
    return 0;
}
