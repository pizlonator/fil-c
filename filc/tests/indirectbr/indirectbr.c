#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int x = 1;

    void** ptrs = malloc(sizeof(void*) * 3);
    ptrs[0] = &&foo;
    ptrs[1] = &&bar;
    ptrs[2] = &&baz;

    goto *ptrs[0];
    ZASSERT(!"Should not be reached.");
    
foo:
    ZASSERT(x == 1);
    x = 2;
    goto *ptrs[1];
    ZASSERT(!"Should not be reached.");

bar:
    ZASSERT(x == 2);
    x = 3;
    goto *ptrs[2];
    ZASSERT(!"Should not be reached.");
    
baz:
    ZASSERT(x == 3);
    return 0;
}

