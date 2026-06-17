#include <stdfil.h>

int main(void)
{
    unsigned long a = 2;
    unsigned long b = 3;
    asm volatile("imul %1, %0"
                 : "+r"(a)
                 : "r"(b));
    ZASSERT(a == 6);
    return 0;
}
