#include <stdfil.h>

int main(void)
{
    long p = 0;
    long y;
    asm("movq (%1), %0" : "=r"(y) : "r"(p) : "memory", "cc");
    zprintf("y = %ld\n", y);
    return 0;
}
