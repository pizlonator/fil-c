#include <stdfil.h>

int main(void)
{
    // Cases where FPREM (truncation) and FPREM1 (IEEE round-to-nearest) agree.
    double a = 7.5;
    double b = 2.5;
    double result;
    asm volatile("fprem1" : "=t"(result) : "0"(a), "u"(b) : "fpsr");
    ZASSERT(result == 0.0);

    double c = 10.0;
    double d = 3.0;
    double result2;
    asm volatile("fprem1" : "=t"(result2) : "0"(c), "u"(d) : "fpsr");
    ZASSERT(result2 == 1.0);

    double e = -7.0;
    double f = 3.0;
    double result3;
    asm volatile("fprem1" : "=t"(result3) : "0"(e), "u"(f) : "fpsr");
    ZASSERT(result3 == -1.0);

    // Case where FPREM and FPREM1 differ: 7.0/4.0 = 1.75.
    // FPREM truncates to 1 and gives remainder 3.0.
    // FPREM1 rounds to 2 and gives remainder -1.0.
    double g = 7.0;
    double h = 4.0;
    double result4;
    asm volatile("fprem1" : "=t"(result4) : "0"(g), "u"(h) : "fpsr");
    ZASSERT(result4 == -1.0);

    return 0;
}
