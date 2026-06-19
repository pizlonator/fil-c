#include <stdfil.h>

int main(void)
{
    asm volatile("sysexit");
    return 0;
}
