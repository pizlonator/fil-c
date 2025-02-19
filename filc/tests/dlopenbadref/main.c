#include <dlfcn.h>
#include <stdfil.h>

int bar = 666;

int main()
{
    void* lib = dlopen("filc/test-output/dlopenbadref/module.so", RTLD_LAZY | RTLD_LOCAL);
    int (*foo)(void) = dlsym(lib, "foo");
    zprintf("foo() = %d\n", foo());
    return 0;
}

