#include <stdfil.h>

static __attribute__((__noinline__)) void* foo(void)
{
    return zcallee();
}

int main()
{
    ZASSERT(foo() == foo);
    return 0;
}

