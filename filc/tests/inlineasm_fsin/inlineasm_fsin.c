#include <math.h>
#include <stdfil.h>

int main(void)
{
    double a = 0.0;
    asm volatile("fsin" : "=t"(a) : "0"(a) : "fpsr");
    ZASSERT(a == 0.0);

    double b = 1.0;
    asm volatile("fsin" : "=t"(b) : "0"(b) : "fpsr");
    ZASSERT(b > 0.84);
    ZASSERT(b < 0.85);

    double c = -1.0;
    asm volatile("fsin" : "=t"(c) : "0"(c) : "fpsr");
    ZASSERT(c > -0.85);
    ZASSERT(c < -0.84);

    return 0;
}
