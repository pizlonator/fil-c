#include <stdfil.h>

int main(void)
{
    unsigned long x = 0;
    asm volatile("incq (%0)" : : "r"(&x));
    return 0;
}
