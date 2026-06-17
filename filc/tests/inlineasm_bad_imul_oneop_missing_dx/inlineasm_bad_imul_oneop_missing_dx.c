#include <stdfil.h>

int main(void)
{
    unsigned long a = 5;
    unsigned long b = 7;
    asm volatile("imulq %1"
                 : "+a"(a)
                 : "r"(b)
                 : "cc");
    return 0;
}
