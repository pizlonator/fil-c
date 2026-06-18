#include <stdfil.h>

typedef float v4sf __attribute__((vector_size(16)));

__attribute__((target("avx"))) int main(void)
{
    v4sf a = { 1.0f, 2.0f, 3.0f, 4.0f };
    v4sf b;

    asm volatile("vmovshdup %1, %0" : "=x"(b) : "x"(a));

    ZASSERT(b[0] == 2.0f);
    ZASSERT(b[1] == 2.0f);
    ZASSERT(b[2] == 4.0f);
    ZASSERT(b[3] == 4.0f);

    return 0;
}
