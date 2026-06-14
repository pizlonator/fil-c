#include <stdfil.h>

int main(void)
{
    int x = 0;
    int y = 5;
    asm("movl %[foo], %[bar]" : [bar] "=r"(x) : [foo] "r"(y));
    ZASSERT(x == 5);
    return 0;
}
