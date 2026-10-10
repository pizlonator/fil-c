#include <dlfcn.h>
#include <stdfil.h>

/* liba.so (linked) and libb.so (dlopened RTLD_LOCAL) both define which().
   A descriptor obtained from libb must run libb's which, as dlsym does
   natively, while references that go through the dynamic symbol table keep
   binding to the first definition in the global scope. */

int which(void);

int main()
{
    void* lib = dlopen("filc/test-output/descriptorlocal/libb.so", RTLD_NOW | RTLD_LOCAL);
    ZASSERT(lib);

    int (*libb_which)(void) = dlsym(lib, "which");
    ZASSERT(libb_which);
    ZASSERT(libb_which() == 2);

    ZASSERT(which() == 1);

    int (*(*whichptr)(void))(void) = dlsym(lib, "whichptr");
    ZASSERT(whichptr);
    ZASSERT(whichptr()() == 1);

    zprintf("descriptorlocal ok\n");
    return 0;
}
