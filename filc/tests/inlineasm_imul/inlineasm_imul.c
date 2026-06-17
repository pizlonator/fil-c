#include <stdfil.h>

int main(void)
{
    unsigned long a, b, lo, hi;
    unsigned __int128 prod;

    /* Two-operand form: dst *= src. */
    a = 6;
    b = 7;
    asm volatile("imul %1, %0"
                 : "+r"(a)
                 : "r"(b)
                 : "cc");
    ZASSERT(a == 42);

    /* Three-operand form: dst = src * imm. */
    asm volatile("imul $11, %1, %0"
                 : "=r"(a)
                 : "r"(b)
                 : "cc");
    ZASSERT(a == 77);

    /* One-operand form: RDX:RAX = RAX * src. */
    lo = 12345;
    b = 6789;
    asm volatile("imulq %2"
                 : "+a"(lo), "=d"(hi)
                 : "r"(b)
                 : "cc");
    prod = (unsigned __int128)12345 * (unsigned __int128)6789;
    ZASSERT(lo == (unsigned long)prod);
    ZASSERT(hi == (unsigned long)(prod >> 64));

    return 0;
}
