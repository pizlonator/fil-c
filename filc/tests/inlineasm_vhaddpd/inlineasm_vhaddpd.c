#include <stdfil.h>

typedef double v2df __attribute__((vector_size(16)));

__attribute__((target("avx"))) int main(void)
{
    v2df a = { 2.0, 1.0 };
    v2df b = { 4.0, 3.0 };
    v2df result;

    asm volatile("vhaddpd %2, %1, %0" : "=x"(result) : "x"(a), "x"(b));

    ZASSERT(result[0] == 3.0);
    ZASSERT(result[1] == 7.0);

    return 0;
}
