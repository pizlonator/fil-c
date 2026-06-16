#include <stdfil.h>

int main(void)
{
    double a = 7.5;
    double b = 2.5;
    double result;
    asm volatile("fprem" : "=t"(result) : "0"(a), "u"(b) : "fpsr");
    ZASSERT(result == 0.0);

    double c = 10.0;
    double d = 3.0;
    double result2;
    asm volatile("fprem" : "=t"(result2) : "0"(c), "u"(d) : "fpsr");
    ZASSERT(result2 == 1.0);

    double e = -7.0;
    double f = 3.0;
    double result3;
    asm volatile("fprem" : "=t"(result3) : "0"(e), "u"(f) : "fpsr");
    ZASSERT(result3 == -1.0);

    return 0;
}
