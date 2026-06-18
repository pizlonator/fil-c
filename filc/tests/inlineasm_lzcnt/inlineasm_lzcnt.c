#include <stdfil.h>

int main(void)
{
    unsigned long src;
    unsigned long dest;

    src = 0x8000000000000000ull;
    asm volatile("lzcntq %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 0);

    src = 0x1000000000000000ull;
    asm volatile("lzcntq %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 3);

    src = 0x1;
    asm volatile("lzcntq %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 63);

    src = 0;
    asm volatile("lzcntq %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 64);

    unsigned int src32;
    unsigned int dest32;
    src32 = 0x80000000;
    asm volatile("lzcntl %1, %0"
                 : "=r"(dest32)
                 : "r"(src32)
                 : "cc");
    ZASSERT(dest32 == 0);

    src32 = 0x10000000;
    asm volatile("lzcntl %1, %0"
                 : "=r"(dest32)
                 : "r"(src32)
                 : "cc");
    ZASSERT(dest32 == 3);

    src32 = 0;
    asm volatile("lzcntl %1, %0"
                 : "=r"(dest32)
                 : "r"(src32)
                 : "cc");
    ZASSERT(dest32 == 32);

    return 0;
}
