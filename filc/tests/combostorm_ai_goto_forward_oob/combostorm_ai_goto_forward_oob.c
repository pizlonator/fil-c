#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc, char** argv)
{
    char* p = opaque(malloc(16));
    if (argc > 1)
        goto skip;
    p[16] = 'x';
skip:
    p[15] = 'y';
    return 0;
}
