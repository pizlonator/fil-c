#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc, char** argv)
{
    int* p = opaque(malloc(16));
    int* q;
    if (argc > 1)
        q = p + 3;
    else
        q = p + 4;
    *q = 42;
    return 0;
}
