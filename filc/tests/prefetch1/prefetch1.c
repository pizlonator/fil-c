#include "utils.h"
#include <stdlib.h>

int main()
{
    void* ptr = opaque((void*)666);
    __builtin_prefetch(ptr, 0, 0);
    __builtin_prefetch(ptr, 1, 0);
    ptr = malloc(16);
    __builtin_prefetch(ptr, 0, 0);
    __builtin_prefetch(ptr, 1, 0);
    int* ptr2 = opaque((void*)666);
    __builtin_prefetch(ptr2, 0, 0);
    __builtin_prefetch(ptr2, 1, 0);
    ptr2 = malloc(16);
    __builtin_prefetch(ptr2, 0, 0);
    __builtin_prefetch(ptr2, 1, 0);
    return 0;
}

