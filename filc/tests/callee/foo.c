#include <stdfil.h>

void foo(void* ptr)
{
    void* callee = zcallee();
    ZASSERT(callee == ptr);
    ZASSERT(zgetlower(callee) == zgetlower(ptr));
}

