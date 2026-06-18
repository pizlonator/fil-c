#include <stdfil.h>

typedef float v4sf __attribute__((vector_size(16)));

int main(void)
{
    v4sf a = { 4.0f, 3.0f, 2.0f, 1.0f };
    v4sf b = { 8.0f, 7.0f, 6.0f, 5.0f };

    asm volatile("movhlps %1, %0" : "+x"(a) : "x"(b));

    ZASSERT(a[0] == 6.0f);
    ZASSERT(a[1] == 5.0f);
    ZASSERT(a[2] == 2.0f);
    ZASSERT(a[3] == 1.0f);

    return 0;
}
