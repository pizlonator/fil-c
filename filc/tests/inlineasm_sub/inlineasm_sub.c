#include <stdfil.h>

int main(void)
{
    unsigned long x;
    unsigned long y;

    x = 10;
    y = 5;
    asm volatile("sub %1, %0"
                 : "+r"(x)
                 : "r"(y)
                 : "cc");
    ZASSERT(x == 5);

    x = 0;
    y = 0;
    asm volatile("sub %1, %0"
                 : "+r"(x)
                 : "r"(y)
                 : "cc");
    ZASSERT(x == 0);

    return 0;
}
