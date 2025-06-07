#include <sys/stat.h>
#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>
#include "utils.h"

int main()
{
    int fd = open("filc/tests/fstatfail6/fstatfail6.c", O_RDONLY);
    ZASSERT(fd >= 0);
    
    struct stat* st = opaque(malloc(sizeof(struct stat)));
    st -= (uintptr_t)st;
    st += UINTPTR_MAX - 10;
    
    fstat(fd, st);
    return 0;
}