#include <stdfil.h>

int main(void)
{
    asm volatile("sysret");
    return 0;
}
