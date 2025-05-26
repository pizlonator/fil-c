#include <stdfil.h>

static int foo(void)
{
    return *(int*)zclosure_get_data(zcallee());
}

int main()
{
    unsigned i;
    for (i = 100000; i--;) {
        int* ptr = zgc_alloc(sizeof(int));
        *ptr = i;
        int (*foo_closure)(void) = zclosure_new(foo, ptr);
        ZASSERT(foo_closure() == i);
    }
    return 0;
}

