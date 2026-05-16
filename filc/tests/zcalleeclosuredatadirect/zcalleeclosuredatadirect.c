#include <stdfil.h>

static __attribute__((__noinline__)) void* foo(void)
{
    return zcallee_closure_data();
}

int main()
{
    foo();
    return 0;
}

