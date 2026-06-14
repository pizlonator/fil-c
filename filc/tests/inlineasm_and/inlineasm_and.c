#include <stdfil.h>

int main(void)
{
    unsigned long x;
    unsigned long y;

    x = 0xff;
    y = 0x0f;
    asm volatile("and %1, %0"
                 : "+r"(x)
                 : "r"(y)
                 : "cc");
    ZASSERT(x == 0x0f);

    x = 0xff;
    y = 0x00;
    asm volatile("and %1, %0"
                 : "+r"(x)
                 : "r"(y)
                 : "cc");
    ZASSERT(x == 0);

    return 0;
}
