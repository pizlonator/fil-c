#include <stdfil.h>

int main(void)
{
    unsigned long src;
    unsigned long dest;

    src = 0x8000000000000000ull;
    asm volatile("popcntq %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 1);

    src = 0x1000000000000000ull;
    asm volatile("popcntq %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 1);

    src = 0x1;
    asm volatile("popcntq %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 1);

    src = 0xffffffffffffffffull;
    asm volatile("popcntq %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 64);

    src = 0;
    asm volatile("popcntq %1, %0"
                 : "=r"(dest)
                 : "r"(src)
                 : "cc");
    ZASSERT(dest == 0);

    unsigned int src32;
    unsigned int dest32;
    src32 = 0xffffffff;
    asm volatile("popcntl %1, %0"
                 : "=r"(dest32)
                 : "r"(src32)
                 : "cc");
    ZASSERT(dest32 == 32);

    src32 = 0x80000000;
    asm volatile("popcntl %1, %0"
                 : "=r"(dest32)
                 : "r"(src32)
                 : "cc");
    ZASSERT(dest32 == 1);

    src32 = 0;
    asm volatile("popcntl %1, %0"
                 : "=r"(dest32)
                 : "r"(src32)
                 : "cc");
    ZASSERT(dest32 == 0);

    return 0;
}
