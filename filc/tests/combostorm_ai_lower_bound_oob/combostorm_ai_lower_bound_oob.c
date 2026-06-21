#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc, char** argv)
{
    char* p = opaque(malloc(16));
    char* q;
    if (argc > 1)
        q = p + 15;
    else
        q = p - 1;
    *q = 'x';
    return 0;
}
