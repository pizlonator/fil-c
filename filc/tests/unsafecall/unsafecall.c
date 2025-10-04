#include <stdfil.h>

int z;

asm(".filc_unsafe_export z");

int main()
{
    z = 1410;
    ZASSERT(zunsafe_call("foo", 42, 666) == 2118);
    return 0;
}

