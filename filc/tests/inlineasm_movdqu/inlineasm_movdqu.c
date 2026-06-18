#include <stdfil.h>

typedef int v4si __attribute__((vector_size(16)));

int main(void)
{
    v4si a = { 4, 3, 2, 1 };
    v4si b = { 8, 7, 6, 5 };

    asm volatile("movdqu %1, %0" : "=x"(a) : "x"(b));

    ZASSERT(a[0] == 8);
    ZASSERT(a[1] == 7);
    ZASSERT(a[2] == 6);
    ZASSERT(a[3] == 5);

    return 0;
}
