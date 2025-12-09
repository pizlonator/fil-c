#include <string.h>
#include <stdfil.h>

extern void* __cpu_model;
extern void* __cpu_features2;

static __attribute__((noinline)) void foo(void)
{
    __cpu_model = strdup("hello");
    __cpu_features2 = strdup("world");
}

int main()
{
    foo();
    zgc_request_and_wait();
    ZASSERT(!strcmp(__cpu_model, "hello"));
    ZASSERT(!strcmp(__cpu_features2, "world"));
    return 0;
}

