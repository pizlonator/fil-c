#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc, char** argv)
{
    char* p = opaque(malloc(16));
    if (argc > 1)
        p[15] = 'a';
    p[16] = 'b';
    return 0;
}
