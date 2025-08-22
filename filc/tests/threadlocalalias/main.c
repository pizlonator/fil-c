#include "header.h"
#include <stdfil.h>
#include "utils.h"

int main()
{
    ZASSERT(x == 666);
    ZASSERT(y == 666);
    x = 42;
    opaque(0); /* FIXME: The compiler is incorrectly assuming that different extern threadlocals do
                  not alias. */
    ZASSERT(x == 42);
    ZASSERT(y == 42);
    return 0;
}

