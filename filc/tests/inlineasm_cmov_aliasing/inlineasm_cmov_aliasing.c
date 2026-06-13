#include <stdfil.h>

int main(void)
{
    int x = 10;
    /* Source and destination are the same placeholder (x). The condition is
       true because 1 != 0, so x is written back with its current value. */
    asm("cmpl $0, %1\n\t"
        "cmovne %0, %0"
        : "+r"(x)
        : "r"(1)
        : "cc");
    zprintf("x = %d\n", x);
    ZASSERT(x == 10);
    return 0;
}
