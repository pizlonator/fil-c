#include <stdfil.h>

static int foo(void)
{
    return *(int*)zclosure_get_data();
}

int main()
{
    int* p = zgc_alloc(sizeof(int));
    *p = 1410;
    int (*foo_closure)(void) = zclosure_new(foo, p);
    ZASSERT(foo_closure() == 1410);
    zgc_free(p);
    zprintf("got this far\n");
    ZASSERT(foo_closure() == 1410);
    return 0;
}

