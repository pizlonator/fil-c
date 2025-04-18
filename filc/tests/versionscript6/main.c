#include <stdfil.h>

extern __thread int foo;

int main()
{
    ZASSERT(foo == 666);
    return 0;
}

