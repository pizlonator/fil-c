#include <stdfil.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "utils.h"

int main()
{
    char* p = malloc(16);
    strcpy(p, "hello");
    opaque(p);
    p -= (uintptr_t)p;
    p += UINTPTR_MAX - 5;
    zsetproctitle(p);
    return 0;
}
