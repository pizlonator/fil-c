#include <stdfil.h>

int main(void)
{
    unsigned long x;
    unsigned long y;

    x = 0xf0;
    y = 0x0f;
    asm volatile("or %1, %0"
                 : "+r"(x)
                 : "r"(y)
                 : "cc");
    ZASSERT(x == 0xff);

    x = 0xff;
    y = 0x00;
    asm volatile("or %1, %0"
                 : "+r"(x)
                 : "r"(y)
                 : "cc");
    ZASSERT(x == 0xff);

    x = 0x00;
    y = 0x00;
    asm volatile("or %1, %0"
                 : "+r"(x)
                 : "r"(y)
                 : "cc");
    ZASSERT(x == 0);

    /* Test different widths. */
    unsigned int a = 0x12345678;
    unsigned int a2 = 0x87654321u;
    asm volatile("orl %1, %0"
                 : "+r"(a)
                 : "r"(a2)
                 : "cc");
    ZASSERT(a == 0x97755779u);

    unsigned short s = 0xf0f0;
    unsigned short s2 = 0x0f0f;
    asm volatile("orw %1, %0"
                 : "+r"(s)
                 : "r"(s2)
                 : "cc");
    ZASSERT(s == 0xffff);

    return 0;
}
