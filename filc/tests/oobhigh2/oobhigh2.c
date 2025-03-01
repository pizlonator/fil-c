#include <stdlib.h>
#include <inttypes.h>
#include "utils.h"

int main()
{
    char* p = opaque(malloc(16));
    p -= (uintptr_t)p;
    p += 18446744073709551614lu;
    *(int*)p = 42;
    return 0;
}
