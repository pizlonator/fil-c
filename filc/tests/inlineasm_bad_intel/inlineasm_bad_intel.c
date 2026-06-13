#include <stdfil.h>

int main(void)
{
    int x = 1;
    int y = 2;
    asm volatile("mov %0, %1" : "+r"(x) : "r"(y));
    zprintf("x = %d\n", x);
    return 0;
}
