#include "header.h"

int main()
{
    void* p = thingy();
    Foo foo(666);
    void* q = foo.bar();
    printf("p = %p\n", p);
    printf("q = %p\n", q);
    fflush(stdout);
    ASSERT(p == q);
    return 0;
}

