#include <stdfil.h>

int main(void)
{
    double a = 1.0;
    double b = 2.0;
    asm volatile("fxch" : "=t"(a), "=u"(b) : "0"(a), "1"(b) : "fpsr");
    ZASSERT(a == 2.0);
    ZASSERT(b == 1.0);

    double c = 3.0;
    double d = 4.0;
    asm volatile("fxch" : "=t"(c), "=u"(d) : "0"(c), "1"(d) : "fpsr");
    ZASSERT(c == 4.0);
    ZASSERT(d == 3.0);

    return 0;
}
