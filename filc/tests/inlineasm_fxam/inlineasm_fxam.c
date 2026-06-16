#include <stdfil.h>

int main(void)
{
    long double a = 1.0L;
    unsigned short sw;
    asm volatile("fxam\n\tfnstsw %0" : "=a"(sw) : "t"(a) : "fpsr");
    ZASSERT(a == 1.0L);
    /* Positive normal finite number: C3=0, C2=1, C1=0, C0=0 */
    ZASSERT((sw & 0x4700) == 0x0400);

    a = -1.0L;
    asm volatile("fxam\n\tfnstsw %0" : "=a"(sw) : "t"(a) : "fpsr");
    ZASSERT(a == -1.0L);
    /* Negative normal finite number: C3=0, C2=1, C1=1, C0=0 */
    ZASSERT((sw & 0x4700) == 0x0600);

    a = 0.0L;
    asm volatile("fxam\n\tfnstsw %0" : "=a"(sw) : "t"(a) : "fpsr");
    ZASSERT(a == 0.0L);
    /* Positive zero: C3=1, C2=0, C1=0, C0=0 */
    ZASSERT((sw & 0x4700) == 0x4000);

    return 0;
}
