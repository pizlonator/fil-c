#include <stdfil.h>
#include <stdint.h>

int main(void)
{
    uint32_t a, b, c, d;
    asm("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d));
    zprintf("a=%u b=%u c=%u d=%u\n", a, b, c, d);
    return 0;
}
