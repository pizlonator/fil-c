#include <stdfil.h>
#include <string.h>

static void foo(const char* str)
{
    void* p = zcallee_closure_data();
    zprintf("p = %P\n", p);
    ZASSERT(!strcmp(p, str));
}

int main()
{
    void (*foo_closure)(const char* str) = zclosure_new(foo, "hello");
    foo_closure("hello");
    zclosure_set_data(foo_closure, "world");
    foo_closure("world");
    return 0;
}

