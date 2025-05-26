#include <stdfil.h>

static void foo(void)
{
    ZASSERT(zclosure_get_data(zcallee()) == (void*)666);
}

int main()
{
    void (*foo_closure)(void) = zclosure_new(foo, (void*)666);
    foo_closure();
    return 0;
}

