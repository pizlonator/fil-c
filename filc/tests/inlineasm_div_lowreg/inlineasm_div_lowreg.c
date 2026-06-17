#include <stdfil.h>

int main(void)
{
    register unsigned char x asm("rdi") = 7;
    unsigned short ax = 100;
    asm volatile("div %%dil" : "+a"(ax) : "r"(x) : "cc");
    ZASSERT((ax & 0xff) == 14);
    ZASSERT((ax >> 8) == 2);
    ZASSERT(x == 7);
    return 0;
}
