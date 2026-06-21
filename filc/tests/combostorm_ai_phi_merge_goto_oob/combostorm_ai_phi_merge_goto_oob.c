#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(void)
{
    char* p = opaque(malloc(16));
    char* q = p;
    int i = 0;
loop:
    if (i == 0)
        q = p + 15;
    else
        q = p + 16;
    i++;
    if (i < 2)
        goto loop;
    *q = 'x';
    return 0;
}
