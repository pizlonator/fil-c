#include <stdfil.h>

/* SAL is an alias for SHL (shift arithmetic/logical left). Verify it is
 * accepted and behaves like a left shift across the operand forms: immediate
 * count, CL count, and the implicit-count-by-1 single-operand form. */

int main(void)
{
    unsigned int x;

    /* Immediate count. */
    x = 1;
    asm("sall $3, %0" : "+r"(x) : : "cc");
    ZASSERT(x == 8);

    /* CL count via "c" input constraint. */
    x = 1;
    unsigned char cnt = 4;
    asm("sall %1, %0" : "+r"(x) : "c"(cnt) : "cc");
    ZASSERT(x == 16);

    /* Single-operand form (shift by 1). */
    x = 1;
    asm("sall %0" : "+r"(x) : : "cc");
    ZASSERT(x == 2);

    /* 64-bit operand. */
    unsigned long y = 1;
    asm("salq $40, %0" : "+r"(y) : : "cc");
    ZASSERT(y == (1UL << 40));

    zprintf("sal ok\n");
    return 0;
}
