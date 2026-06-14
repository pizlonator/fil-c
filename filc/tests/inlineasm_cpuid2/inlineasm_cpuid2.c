#include <stdfil.h>

int main()
{
    unsigned f1c = 0;
    unsigned f1d = 0;
    unsigned f7b = 0;
    unsigned f7c = 0;
    unsigned n;
    __asm__("cpuid" : "=a"(n) : "a"(0) : "ebx", "ecx", "edx");
    if (n >= 1) {
      unsigned f1a;
      __asm__("cpuid" : "=a"(f1a), "=c"(f1c), "=d"(f1d) : "a"(1) : "ebx");
    }
    if (n >= 7) {
      unsigned f7a;
      __asm__("cpuid"
              : "=a"(f7a), "=b"(f7b), "=c"(f7c)
              : "a"(7), "c"(0)
              : "edx");
    }
    zprintf("%u, %u, %u, %u\n", f1c, f1d, f7b, f7c);
    return 0;
}

