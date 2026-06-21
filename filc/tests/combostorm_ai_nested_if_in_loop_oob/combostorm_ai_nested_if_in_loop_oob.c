#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc, char** argv)
{
    char* p = opaque(malloc(16));
    for (int i = 0; i < 2; i++) {
        if (argc > 1) {
            if (i == 0)
                p[15] = 'a';
        }
    }
    p[16] = 'b';
    return 0;
}
