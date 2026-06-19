#include <stdfil.h>

int main(void)
{
    unsigned long x;
    unsigned long dummy;

    /* cmp $1, dummy sets CF when dummy < 1. sbb then subtracts the
       immediate, the destination, and CF: dest = dest - src - CF. */

    /* dummy == 1: CF = 0, so sbb $5, x with x = 10 gives 5. */
    x = 10;
    dummy = 1;
    asm volatile("cmp $1, %1\n\t"
                 "sbb $5, %0"
                 : "+r"(x)
                 : "r"(dummy)
                 : "cc");
    ZASSERT(x == 5);

    /* dummy == 0: CF = 1, so sbb $5, x with x = 10 gives 4. */
    x = 10;
    dummy = 0;
    asm volatile("cmp $1, %1\n\t"
                 "sbb $5, %0"
                 : "+r"(x)
                 : "r"(dummy)
                 : "cc");
    ZASSERT(x == 4);

    return 0;
}
