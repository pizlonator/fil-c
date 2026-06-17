#include <stdfil.h>

int main(void)
{
    unsigned long b = 3;
    asm volatile("imul %0, %%rax"
                 :
                 : "r"(b)
                 : "cc");
    return 0;
}
