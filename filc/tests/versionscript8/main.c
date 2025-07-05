#include <dlfcn.h>
#include <stdfil.h>

void* opaque(void*);

int main()
{
    void* lib = dlopen("filc/test-output/versionscript8/libtest.so", RTLD_LAZY | RTLD_LOCAL);
    ZASSERT(lib);

    ZASSERT(dlsym(lib, "foo"));
    ZASSERT(dlsym(lib, "baz"));
    ZASSERT(dlsym(lib, "thingy"));
    ZASSERT(dlsym(lib, "bar_baz_foo"));
    ZASSERT(dlsym(lib, "ThingyStuff"));

    ZASSERT(!dlsym(lib, "bar"));
    ZASSERT(!dlsym(lib, "stuff"));
    ZASSERT(!dlsym(lib, "foo_bar_baz"));
    ZASSERT(!dlsym(lib, "StuffThingy"));

    return 0;
}

