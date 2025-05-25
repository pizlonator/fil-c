#include <stdfil.h>
#include <string.h>

static void foo(void)
{
    ZASSERT(!strcmp(zclosure_get_data(), "hello"));
}

int main()
{
    void (*foo_closure)(void) = zclosure_new(foo, "hello");
    foo_closure();
    return 0;
}

