#include <stdfil.h>

int main(void)
{
    int x = 3;
    asm("incl %0" : "+r"(x));
    zprintf("x = %d\n", x);
    return 0;
}
