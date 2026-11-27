#include <dlfcn.h>
#include <stdfil.h>
#include <stdio.h>

int main(void)
{
    void *handle = dlopen("filc/test-output/wlversionscript/libwlvs.so",
                          RTLD_LAZY | RTLD_LOCAL);
    ZASSERT(handle);

    int (*exported_fn)(void) = dlsym(handle, "exported_fn");
    ZASSERT(exported_fn);
    ZASSERT(exported_fn() == 42);

    void *hidden_fn = dlsym(handle, "hidden_fn");
    ZASSERT(!hidden_fn);

    dlclose(handle);
    printf("wlversionscript ok\n");
    return 0;
}
