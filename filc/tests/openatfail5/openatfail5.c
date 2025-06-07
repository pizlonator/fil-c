#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "utils.h"

int main()
{
    char* p = opaque(malloc(16));
    strcpy(p, "test.txt");
    p -= (uintptr_t)p;
    p += UINTPTR_MAX - 10;
    
    openat(AT_FDCWD, p, O_RDONLY);
    return 0;
}