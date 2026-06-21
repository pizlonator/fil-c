#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc, char** argv)
{
    char* base = opaque(malloc(16));
    size_t a = (argc > 1) ? 0 : 15;
    size_t b = (argc > 1) ? 1 : 16;
    char* p = base + a;
    char* q;
    if (argc > 100)
        q = p;
    else
        q = base + b;
    *q = 'x';
    return 0;
}
