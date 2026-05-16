#include <stdfil.h>

static __attribute__((__noinline__)) void* foo(void)
{
    return zcallee_closure_data();
}

int main()
{
    zprintf("Hello\n");
    foo();
    return 0;
}

