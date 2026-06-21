#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main(void)
{
    char* p = opaque(malloc(16));
    char sum = 0;
    for (int i = 0; i < 20; i++)
        sum += p[i];
    ZASSERT(sum == 0);
    return 0;
}
