#include <stdfil.h>

typedef double v2df __attribute__((vector_size(16)));

int main(void)
{
    v2df a = { 1.0, 2.0 };
    v2df b;

    asm volatile("movddup %1, %0" : "=x"(b) : "x"(a));

    ZASSERT(b[0] == 1.0);
    ZASSERT(b[1] == 1.0);

    return 0;
}
