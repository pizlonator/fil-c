#include <stdfil.h>
#include <stdlib.h>

int main()
{
    void** ptrs = malloc(sizeof(void*) * 3);
    ptrs[0] = &&foo;
    ptrs[1] = &&bar;
    ptrs[2] = &&baz;
    return 0;
    
foo:
    ZASSERT(!"Should not be reached.");

bar:
    ZASSERT(!"Should not be reached.");
    
baz:
    ZASSERT(!"Should not be reached.");
    return 1;
}

