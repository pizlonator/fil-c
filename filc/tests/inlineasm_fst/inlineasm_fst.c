#include <stdfil.h>

int main(void)
{
    double a = 3.14;
    double b = 0.0;
    asm volatile("fst %%st(1)" : "=t"(a), "=u"(b) : "0"(a));
    ZASSERT(a == 3.14);
    ZASSERT(b == 3.14);

    a = -2.71;
    b = 0.0;
    asm volatile("fst %%st(1)" : "=t"(a), "=u"(b) : "0"(a));
    ZASSERT(a == -2.71);
    ZASSERT(b == -2.71);

    return 0;
}
