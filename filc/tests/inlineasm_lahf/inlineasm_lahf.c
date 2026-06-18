#include <stdfil.h>

int main(void)
{
    unsigned short ax;
    unsigned long dummy;
    unsigned char ah;

    /* cmp $1, 1: result is zero, so ZF = 1 and CF = 0. */
    dummy = 1;
    asm volatile("cmp $1, %1\n\t"
                 "lahf"
                 : "=a"(ax)
                 : "r"(dummy)
                 : "cc");
    ah = ax >> 8;
    ZASSERT((ah & 0x40) == 0x40); /* ZF */
    ZASSERT((ah & 0x01) == 0);    /* CF */

    /* cmp $2, 1: 1 - 2 borrows, so CF = 1. */
    dummy = 1;
    asm volatile("cmp $2, %1\n\t"
                 "lahf"
                 : "=a"(ax)
                 : "r"(dummy)
                 : "cc");
    ah = ax >> 8;
    ZASSERT((ah & 0x01) == 0x01); /* CF */

    /* cmp $1, 0: 0 - 1 is negative, so SF = 1. */
    dummy = 0;
    asm volatile("cmp $1, %1\n\t"
                 "lahf"
                 : "=a"(ax)
                 : "r"(dummy)
                 : "cc");
    ah = ax >> 8;
    ZASSERT((ah & 0x80) == 0x80); /* SF */

    return 0;
}
