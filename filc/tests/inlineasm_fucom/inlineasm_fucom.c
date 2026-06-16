#include <stdfil.h>

int main(void)
{
    double a = 1.0;
    double b = 2.0;
    asm volatile("fucom" : : "t"(a), "u"(b) : "fpsr");
    ZASSERT(a == 1.0);
    ZASSERT(b == 2.0);

    a = 3.0;
    b = 3.0;
    asm volatile("fucom" : : "t"(a), "u"(b) : "fpsr");
    ZASSERT(a == 3.0);
    ZASSERT(b == 3.0);

    return 0;
}
