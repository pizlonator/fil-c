#include <stdfil.h>
#include <filc_test_support.h>
#include "utils.h"

int main()
{
    int x;
    int y = 666;
    try {
        zmemmove_union((char*)opaque(&x) + 16, &y, sizeof(int));
    } catch (int& e) {
        x = 1410;
    }
    zprintf("x = %d\n", x);
    return 0;
}

