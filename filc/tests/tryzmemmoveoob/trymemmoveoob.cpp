#include <stdfil.h>
#include "utils.h"

int main()
{
    int x;
    int y = 666;
    try {
        zmemmove((char*)opaque(&x) + 16, &y, sizeof(int));
    } catch (int& e) {
        x = 1410;
    }
    zprintf("x = %d\n", x);
    return 0;
}

