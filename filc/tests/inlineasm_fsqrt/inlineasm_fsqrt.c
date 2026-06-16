#include <math.h>
#include <stdfil.h>

int main(void)
{
    double a = 0.0;
    asm volatile("fsqrt" : "=t"(a) : "0"(a) : "fpsr");
    ZASSERT(a == 0.0);

    double b = 4.0;
    asm volatile("fsqrt" : "=t"(b) : "0"(b) : "fpsr");
    ZASSERT(b == 2.0);

    double c = 9.0;
    asm volatile("fsqrt" : "=t"(c) : "0"(c) : "fpsr");
    ZASSERT(c == 3.0);

    double d = 2.0;
    asm volatile("fsqrt" : "=t"(d) : "0"(d) : "fpsr");
    ZASSERT(d > 1.41);
    ZASSERT(d < 1.42);

    return 0;
}
