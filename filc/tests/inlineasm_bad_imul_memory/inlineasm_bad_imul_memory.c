#include <stdfil.h>

int main(void)
{
    unsigned long a = 2;
    unsigned long p = 0;
    asm volatile("imul (%1), %0"
                 : "+r"(a)
                 : "r"(p)
                 : "cc");
    return 0;
}
