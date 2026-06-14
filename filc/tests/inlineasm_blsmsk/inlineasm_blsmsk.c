#include <stdfil.h>

int main(void)
{
    unsigned long src;
    unsigned long dest;

    src = 0x110;
    asm volatile("blsmsk %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 0x1f);

    src = 0x1;
    asm volatile("blsmsk %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 0x1);

    src = 0;
    asm volatile("blsmsk %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 0xffffffffffffffff);

    return 0;
}
