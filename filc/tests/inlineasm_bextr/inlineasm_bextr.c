#include <stdfil.h>

int main(void)
{
    unsigned long src;
    unsigned long control;
    unsigned long dest;

    src = 0x12345678;
    control = 8 | (8 << 8);
    asm volatile("bextr %1, %2, %0"
                 : "=r"(dest)
                 : "r"(control), "r"(src)
                 : "cc");
    ZASSERT(dest == 0x56);

    src = 0xffffffff;
    control = 0 | (8 << 8);
    asm volatile("bextr %1, %2, %0"
                 : "=r"(dest)
                 : "r"(control), "r"(src)
                 : "cc");
    ZASSERT(dest == 0xff);

    src = 0;
    control = 0 | (8 << 8);
    asm volatile("bextr %1, %2, %0"
                 : "=r"(dest)
                 : "r"(control), "r"(src)
                 : "cc");
    ZASSERT(dest == 0);

    return 0;
}
