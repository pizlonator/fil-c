#include <stdfil.h>

int main(void)
{
    int x = 0x8000;
    int s = 4;
    asm("sarw %%cl, %0" : "+r"(x) : "r"(s) : "cc");
    zprintf("x = %d\n", x);
    return 0;
}
