#include <stdfil.h>

int main(void)
{
    long x, y;
    asm volatile("leaq 0(%0), %1" : "=r"(y) : "r"(x));
    zprintf("y = %ld\n", y);
    return 0;
}
