#include <stdfil.h>

int main(void)
{
    double a = 2.3;
    asm volatile("frndint" : "=t"(a) : "0"(a) : "fpsr");
    ZASSERT(a == 2.0);

    double b = 3.7;
    asm volatile("frndint" : "=t"(b) : "0"(b) : "fpsr");
    ZASSERT(b == 4.0);

    double c = -2.3;
    asm volatile("frndint" : "=t"(c) : "0"(c) : "fpsr");
    ZASSERT(c == -2.0);

    double d = -3.7;
    asm volatile("frndint" : "=t"(d) : "0"(d) : "fpsr");
    ZASSERT(d == -4.0);

    return 0;
}
