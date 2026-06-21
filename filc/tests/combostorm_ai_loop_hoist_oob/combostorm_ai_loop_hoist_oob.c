#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(void)
{
    char* p = opaque(malloc(16));
    for (int i = 0; i < 20; i++)
        p[i] = 'x';
    return 0;
}
