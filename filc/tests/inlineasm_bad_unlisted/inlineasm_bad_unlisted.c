#include <stdfil.h>

int main(void)
{
    int x = 42;
    asm("addw $1,%0" : "+r"(x) : : "cc");
    zprintf("x = %d\n", x);
    return 0;
}
