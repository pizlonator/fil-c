#include <stdfil.h>

int main(void)
{
    asm volatile("sysenter");
    return 0;
}
