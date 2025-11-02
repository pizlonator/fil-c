#include <stdfil.h>

static __attribute__((noinline)) int foo(int* p, float* q)
{
    int x = *p;
    *q = 0;
    return x + *p;
}

int main()
{
    int x = 42;
    ZASSERT(foo(&x, (float*)&x) == 42);
    return 0;
}

