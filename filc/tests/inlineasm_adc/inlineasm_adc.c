#include <stdfil.h>

int main(void)
{
    unsigned long x;
    unsigned long dummy;

    /* cmp $1, dummy sets CF when dummy < 1. adc then adds the immediate,
       the destination, and CF. */

    /* dummy == 1: CF = 0, so adc $5, x with x = 10 gives 15. */
    x = 10;
    dummy = 1;
    asm volatile("cmp $1, %1\n\t"
                 "adc $5, %0"
                 : "+r"(x)
                 : "r"(dummy)
                 : "cc");
    ZASSERT(x == 15);

    /* dummy == 0: CF = 1, so adc $5, x with x = 10 gives 16. */
    x = 10;
    dummy = 0;
    asm volatile("cmp $1, %1\n\t"
                 "adc $5, %0"
                 : "+r"(x)
                 : "r"(dummy)
                 : "cc");
    ZASSERT(x == 16);

    return 0;
}
