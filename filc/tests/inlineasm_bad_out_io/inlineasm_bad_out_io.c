#include <stdfil.h>

int main(void)
{
    asm volatile("outb %%al, $0x80" : : "a"(0));
    return 0;
}
