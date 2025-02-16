#include <stdfil.h>

int bar(void);
int thingy(void);

int main()
{
    ZASSERT(bar() == 666);
    ZASSERT(thingy() == 1410);
    return 0;
}
