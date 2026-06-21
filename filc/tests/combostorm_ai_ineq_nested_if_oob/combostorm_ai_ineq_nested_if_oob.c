#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc, char** argv)
{
    char* p = opaque(malloc(16));
    if (argc > 1) {
        if (argc > 2)
            p[15] = 'x';
    }
    p[16] = 'y';
    return 0;
}
