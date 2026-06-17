#include <stdfil.h>

typedef float v4sf __attribute__((vector_size(16)));

__attribute__((target("avx"))) int main(void)
{
    v4sf a = { 1.0f, 2.0f, 3.0f, 4.0f };
    v4sf b = { 5.0f, 6.0f, 7.0f, 8.0f };
    v4sf result;

    asm volatile("vhsubps %2, %1, %0" : "=x"(result) : "x"(a), "x"(b));

    ZASSERT(result[0] == -1.0f);
    ZASSERT(result[1] == -1.0f);
    ZASSERT(result[2] == -1.0f);
    ZASSERT(result[3] == -1.0f);

    return 0;
}
