#include <stdfil.h>

int foo(void);
int bar(void);

int main()
{
    ZASSERT(foo() == 1410);
    ZASSERT(bar() == 666);
    return 0;
}


