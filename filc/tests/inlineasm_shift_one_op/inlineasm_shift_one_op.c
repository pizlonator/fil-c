#include <stdfil.h>

int main(void)
{
    unsigned int x = 1;
    asm("shll %0" : "+r"(x) : : "cc");
    zprintf("x = %u\n", x);
    return 0;
}
