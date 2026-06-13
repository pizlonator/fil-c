#include <stdfil.h>

int main(void)
{
    int x = 0;
    int y = 42;
    /* cmovne reads y and writes ebx; the condition is true because 1 != 0. */
    asm("cmpl $0, %2\n\t"
        "cmovne %1, %%ebx"
        : "=b"(x)
        : "r"(y), "r"(1)
        : "cc");
    zprintf("x = %d\n", x);
    ZASSERT(x == 42);
    return 0;
}
