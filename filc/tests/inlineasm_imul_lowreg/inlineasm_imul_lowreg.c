#include <stdfil.h>

int main(void)
{
    register unsigned char x asm("rdi") = 7;
    unsigned short ax = 5;
    asm volatile("imul %%dil" : "+a"(ax) : "r"(x) : "dx", "cc");
    ZASSERT(ax == 35);
    ZASSERT(x == 7);
    return 0;
}
