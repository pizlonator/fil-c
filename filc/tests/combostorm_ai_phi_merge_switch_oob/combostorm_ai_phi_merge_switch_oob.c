#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc, char** argv)
{
    char* p = opaque(malloc(16));
    char* q = p;
    switch (argc % 2) {
    case 0:
        q = p + 15;
        break;
    default:
        q = p + 16;
        break;
    }
    *q = 'x';
    return 0;
}
