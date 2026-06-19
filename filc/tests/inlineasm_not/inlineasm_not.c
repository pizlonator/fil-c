#include <stdfil.h>

int main(void)
{
    unsigned long x64;
    unsigned int x32;
    unsigned short x16;

    x64 = 0x0102030405060708ull;
    asm volatile("notq %0" : "+r"(x64));
    ZASSERT(x64 == ~0x0102030405060708ull);

    x32 = 0x12345678;
    asm volatile("notl %0" : "+r"(x32));
    ZASSERT(x32 == ~0x12345678u);

    x16 = 0x1234;
    asm volatile("notw %0" : "+r"(x16));
    ZASSERT(x16 == (unsigned short)~0x1234u);

    x64 = 0x0102030405060708ull;
    asm volatile("not %0" : "+r"(x64));
    ZASSERT(x64 == ~0x0102030405060708ull);

    return 0;
}
