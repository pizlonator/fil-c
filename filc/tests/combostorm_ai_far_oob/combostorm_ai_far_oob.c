#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc, char** argv)
{
    char* p = opaque(malloc(16));
    size_t off = (argc > 1) ? 15 : 1000;
    char* q = p + off;
    *q = 'x';
    return 0;
}
