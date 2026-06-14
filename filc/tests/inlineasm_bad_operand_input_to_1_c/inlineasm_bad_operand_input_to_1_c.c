#include <stdfil.h>

int main(void)
{
    int x = 42;
    asm("movl %1, %1" : : "r"(x));
    return 0;
}
