#include <stdfil.h>

typedef float v8sf __attribute__((vector_size(32)));

__attribute__((target("avx"))) int main(void)
{
    v8sf a = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
    v8sf b;

    asm volatile("vmovsldup %1, %0" : "=x"(b) : "x"(a));

    ZASSERT(b[0] == 1.0f);
    ZASSERT(b[1] == 1.0f);
    ZASSERT(b[2] == 3.0f);
    ZASSERT(b[3] == 3.0f);
    ZASSERT(b[4] == 5.0f);
    ZASSERT(b[5] == 5.0f);
    ZASSERT(b[6] == 7.0f);
    ZASSERT(b[7] == 7.0f);

    return 0;
}
