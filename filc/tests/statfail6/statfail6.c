#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>
#include "utils.h"

int main()
{
    struct stat* st = opaque(malloc(sizeof(struct stat)));
    st -= (uintptr_t)st;
    st += UINTPTR_MAX - 10;
    
    stat("filc/tests/statfail6/statfail6.c", st);
    return 0;
}