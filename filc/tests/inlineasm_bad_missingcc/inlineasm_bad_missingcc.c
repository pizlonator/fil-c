#include <stdfil.h>

int main(void)
{
    int x = 42;
    asm("sarw $15,%0" : "+r"(x));
    zprintf("x = %d\n", x);
    return 0;
}
