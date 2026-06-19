#include <stdfil.h>

int main(void)
{
    /* SERIALIZE support is indicated by CPUID.07H.0H:EDX.SERIALIZE[bit 14].
       Query leaf 7, subleaf 0 to check whether this CPU supports SERIALIZE.
       The cpuid instruction reads eax/ecx and writes eax/ebx/ecx/edx, so all
       four outputs must be declared. */
    unsigned a, b, c, d;
    __asm__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(7), "c"(0));
    if (d & (1u << 14)) {
        asm volatile("serialize" ::: "memory");
    }
    ZASSERT(1);
    return 0;
}
