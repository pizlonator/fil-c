#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(void)
{
    char* p = opaque(malloc(16));
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++)
            p[i * 16 + j] = 'x';
    }
    return 0;
}
