#include <stdfil.h>

int z;

asm(".filc_unsafe_export z");

int main()
{
    z = 1410;
    ZASSERT(zunsafe_call("foo", 42, 666) == 2118);
    ZASSERT(zunsafe_call("foo", 43, 667) == 2120);
    ZASSERT(zunsafe_fast_call("foo", 44, 668) == 2122);
    ZASSERT(zunsafe_fast_call("foo", 45, 669) == 2124);
    ZASSERT(zunsafe_buf_call(10, "foo", 46, 670) == 2126);
    ZASSERT(zunsafe_buf_call(1000000, "foo", 47, 671) == 2128);
    return 0;
}

