#include <stdfil.h>

int main(void)
{
    asm volatile("tilezero %tmm0");
    return 0;
}
