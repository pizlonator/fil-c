#include <stdfil.h>

int main(void)
{
    unsigned long x;
    unsigned long y;
    unsigned long z;

    /* lea with base register and zero offset should reproduce the input value. */
    x = 42;
    asm("leaq 0(%1), %0" : "=r"(y) : "r"(x));
    ZASSERT(y == x);

    /* lea with a non-zero immediate offset. */
    x = 10;
    asm("leaq 7(%1), %0" : "=r"(y) : "r"(x));
    ZASSERT(y == x + 7);

    /* lea with base + index*scale + offset. */
    x = 3;
    z = 5;
    asm("leaq 8(%1, %2, 2), %0" : "=r"(y) : "r"(x), "r"(z));
    ZASSERT(y == x + z * 2 + 8);

    return 0;
}
