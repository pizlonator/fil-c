#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <stdint.h>

int main()
{
    struct stat* st = malloc(sizeof(struct stat));
    // Cause overflow by adding a large value that will wrap around
    lstat("filc/tests/lstatfail6/lstatfail6.c", (struct stat*)((uintptr_t)st + UINTPTR_MAX));
    return 0;
}