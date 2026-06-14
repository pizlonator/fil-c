#include <stdfil.h>

int main(void)
{
    asm("movl %1, %1");
    return 0;
}
