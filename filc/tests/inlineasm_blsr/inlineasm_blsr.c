#include <stdfil.h>

int main(void)
{
    unsigned long src;
    unsigned long dest;

    src = 0x110;
    asm volatile("blsr %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 0x100);

    src = 0x1;
    asm volatile("blsr %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 0);

    src = 0;
    asm volatile("blsr %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 0);

    return 0;
}
