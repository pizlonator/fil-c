#include <stdfil.h>

typedef float v4sf __attribute__((vector_size(16)));

int main(void)
{
    v4sf a = { 1.0f, 5.0f, 3.0f, -1.0f };
    v4sf b = { 2.0f, 4.0f, 3.0f, 0.0f };

    asm volatile("maxps %1, %0" : "+x"(a) : "x"(b));

    ZASSERT(a[0] == 2.0f);
    ZASSERT(a[1] == 5.0f);
    ZASSERT(a[2] == 3.0f);
    ZASSERT(a[3] == 0.0f);

    return 0;
}
