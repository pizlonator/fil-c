#include <stdfil.h>

int main(void)
{
    // fsubr with explicit operands: ST(0) = ST(1) - ST(0).
    double a = 5.0;
    double b = 2.0;
    double result;
    asm volatile("fsubr %%st(1), %%st" : "=t"(result) : "0"(a), "u"(b) : "fpsr");
    ZASSERT(result == -3.0);

    return 0;
}
