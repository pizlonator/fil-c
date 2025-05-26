#include <stdfil.h>
#include <stdlib.h>

static void foo(void) { }

int main()
{
    zclosure_set_data(foo, NULL);
    return 0;
}

