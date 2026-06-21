#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(void)
{
    char* p = opaque(malloc(16));
    p[15] = 'a';
    p[16] = 'b';
    return 0;
}
