#include <stdfil.h>

typedef double v2df __attribute__((vector_size(16)));

__attribute__((target("sse3"))) int main(void)
{
    v2df a = { 2.0, 1.0 };
    v2df b = { 4.0, 3.0 };

    asm volatile("haddpd %1, %0" : "+x"(a) : "x"(b));

    ZASSERT(a[0] == 3.0);
    ZASSERT(a[1] == 7.0);

    return 0;
}
