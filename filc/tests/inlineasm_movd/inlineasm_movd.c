#include <stdfil.h>

typedef int v4si __attribute__((vector_size(16)));

int main(void)
{
    unsigned x = 0x12345678;
    v4si v;

    asm volatile("movd %1, %0" : "=x"(v) : "r"(x));

    unsigned y;
    asm volatile("movd %1, %0" : "=r"(y) : "x"(v));

    ZASSERT(y == 0x12345678);

    return 0;
}
