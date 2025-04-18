#include <stdfil.h>

extern int foo;

int main()
{
    ZASSERT(foo == 666);
    return 0;
}

