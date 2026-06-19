#include <stdfil.h>

int main(void)
{
    asm volatile("swapgs");
    return 0;
}
