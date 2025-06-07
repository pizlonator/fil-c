#include <unistd.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>
#include "utils.h"

int main()
{
    int* p = opaque(malloc(2 * sizeof(int)));
    p -= (uintptr_t)p;
    p += UINTPTR_MAX - 10;
    
    pipe(p);
    return 0;
}