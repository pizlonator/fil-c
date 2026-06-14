#include <stdfil.h>

int main(void)
{
    unsigned long src1;
    unsigned long src2;
    unsigned long dest;

    src1 = 0xff;
    src2 = 0x0f;
    asm volatile("andn %1, %2, %0"
                 : "=r"(dest)
                 : "r"(src1), "r"(src2));
    ZASSERT(dest == (src1 & ~src2));

    src1 = 0x00;
    src2 = 0xff;
    asm volatile("andn %1, %2, %0"
                 : "=r"(dest)
                 : "r"(src1), "r"(src2));
    ZASSERT(dest == 0);

    return 0;
}
