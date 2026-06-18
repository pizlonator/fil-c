#include <stdfil.h>

typedef float v4sf __attribute__((vector_size(16)));

int main(void)
{
    v4sf a = { 1.0f, 2.0f, 3.0f, 4.0f };
    v4sf b = { 5.0f, 6.0f, 7.0f, 8.0f };

    asm volatile("minss %1, %0" : "+x"(a) : "x"(b));

    ZASSERT(a[0] == 1.0f);
    ZASSERT(a[1] == 2.0f);
    ZASSERT(a[2] == 3.0f);
    ZASSERT(a[3] == 4.0f);

    v4sf c = { 10.0f, 20.0f, 30.0f, 40.0f };
    v4sf d = { 3.0f, 4.0f, 5.0f, 6.0f };

    asm volatile("minss %1, %0" : "+x"(c) : "x"(d));

    ZASSERT(c[0] == 3.0f);
    ZASSERT(c[1] == 20.0f);
    ZASSERT(c[2] == 30.0f);
    ZASSERT(c[3] == 40.0f);

    return 0;
}
