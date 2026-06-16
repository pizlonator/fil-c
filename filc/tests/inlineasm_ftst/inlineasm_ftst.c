#include <stdfil.h>

int main(void)
{
    double a = 1.0;
    unsigned short sw;
    asm volatile("ftst\n\tfnstsw %0" : "=a"(sw) : "t"(a) : "fpsr");
    ZASSERT(a == 1.0);
    ZASSERT((sw & 0x4500) == 0);        /* ST(0) > 0.0 */

    a = -1.0;
    asm volatile("ftst\n\tfnstsw %0" : "=a"(sw) : "t"(a) : "fpsr");
    ZASSERT(a == -1.0);
    ZASSERT((sw & 0x4500) == 0x0100);   /* ST(0) < 0.0 */

    a = 0.0;
    asm volatile("ftst\n\tfnstsw %0" : "=a"(sw) : "t"(a) : "fpsr");
    ZASSERT(a == 0.0);
    ZASSERT((sw & 0x4500) == 0x4000);   /* ST(0) == 0.0 */

    return 0;
}
