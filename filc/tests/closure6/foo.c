#include <stdfil.h>

void foo(void)
{
    zprintf("bad = %P\n", zcallee_closure_data());
}
