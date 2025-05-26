#include <stdfil.h>

void foo(void)
{
    zprintf("bad = %P\n", zclosure_get_data(zcallee()));
}
