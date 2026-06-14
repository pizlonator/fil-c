#include <stdfil.h>

typedef double v2df __attribute__((vector_size(16)));

int main(void)
{
    v2df a = { 2.0, 1.0 };
    v2df b = { 4.0, 3.0 };

    asm volatile("addpd %1, %0" : "+x"(a) : "x"(b));

    ZASSERT(a[0] == 6.0);
    ZASSERT(a[1] == 4.0);

    return 0;
}
