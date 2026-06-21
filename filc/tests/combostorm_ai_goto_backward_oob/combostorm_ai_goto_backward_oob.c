#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(void)
{
    char* p = opaque(malloc(16));
    int count = 0;
loop:
    if (count >= 1)
        goto done;
    p[16] = 'x';
    count++;
    goto loop;
done:
    return 0;
}
