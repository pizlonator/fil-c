#include <stdfil.h>

typedef long long v2di __attribute__((vector_size(16)));

int main(void)
{
    unsigned long long x = 0x123456789abcdef0ULL;
    v2di v;

    asm volatile("movq %1, %0" : "=x"(v) : "r"(x));

    unsigned long long y;
    asm volatile("movq %1, %0" : "=r"(y) : "x"(v));

    ZASSERT(y == 0x123456789abcdef0ULL);

    v2di v2;
    asm volatile("movq %1, %0" : "=x"(v2) : "x"(v));

    unsigned long long z;
    asm volatile("movq %1, %0" : "=r"(z) : "x"(v2));

    ZASSERT(z == 0x123456789abcdef0ULL);

    return 0;
}
