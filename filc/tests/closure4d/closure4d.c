#include <stdfil.h>
#include <stdlib.h>

static int foo(void)
{
    return *(int*)zcallee_closure_data();
}

int main()
{
    unsigned i;
    int (*foo_closure)(void) = zclosure_new(foo, NULL);
    for (i = 100000; i--;) {
        int* ptr = zgc_alloc(sizeof(int));
        *ptr = i;
        zclosure_set_data(foo_closure, ptr);
        ZASSERT(foo_closure() == i);
    }
    return 0;
}

