#include <stdfil.h>

int main(void)
{
    int x = 1;
    unsigned char s = 3;
    asm("shll %1, %0" : "+r"(x) : "c"(s) : "cc");
    zprintf("x = %d\n", x);
    return 0;
}
