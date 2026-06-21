#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc, char** argv)
{
    char* p = opaque(malloc(16));
    switch (argc % 3) {
    case 0:
        p[15] = 'a';
        break;
    case 1:
        p[14] = 'b';
        break;
    default:
        p[13] = 'c';
        break;
    }
    p[16] = 'z';
    return 0;
}
