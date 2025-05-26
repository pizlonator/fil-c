#include <stdfil.h>
#include <stdlib.h>

static void foo(void) { }

int main()
{
    zclosure_get_data(foo);
    return 0;
}

