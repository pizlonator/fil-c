#include <stdfil.h>

int main(void)
{
    asm volatile("syscall");
    return 0;
}
