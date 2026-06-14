#include <stdfil.h>

int main(void)
{
    asm("movl %0, %0");
    return 0;
}
