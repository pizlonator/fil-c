#include <stdfil.h>

int main(void)
{
    unsigned long result;
    asm volatile("imulq $5" : "=a"(result) : : "dx", "cc");
    ZASSERT(result == 0);
    return 0;
}
