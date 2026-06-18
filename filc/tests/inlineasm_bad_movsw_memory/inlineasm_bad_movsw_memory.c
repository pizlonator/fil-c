#include <stdfil.h>

int main(void)
{
    asm volatile("movsw" ::: "rsi", "rdi", "memory");
    return 0;
}
