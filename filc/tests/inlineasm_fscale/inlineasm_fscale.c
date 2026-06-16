#include <stdfil.h>

int main(void)
{
    double a = 1.5;
    double b = 3.0;
    double result;
    asm volatile("fscale" : "=t"(result) : "0"(a), "u"(b) : "fpsr");
    ZASSERT(result == 12.0);

    double c = 5.0;
    double d = 2.0;
    double result2;
    asm volatile("fscale" : "=t"(result2) : "0"(c), "u"(d) : "fpsr");
    ZASSERT(result2 == 20.0);

    double e = 8.0;
    double f = -1.0;
    double result3;
    asm volatile("fscale" : "=t"(result3) : "0"(e), "u"(f) : "fpsr");
    ZASSERT(result3 == 4.0);

    return 0;
}
