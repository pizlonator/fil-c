#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(void)
{
    char* p = opaque(malloc(16));
    for (int i = 0; i < 16; i++)
        p[i] = 'x';
    p[16] = 'y';
    return 0;
}
