#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>

int main()
{
    struct stat* st = malloc(sizeof(struct stat));
    // Cause overflow by adding a large value that will wrap around
    fstatat(AT_FDCWD, "filc/tests/fstatatfail6/fstatatfail6.c", (struct stat*)((uintptr_t)st + UINTPTR_MAX), 0);
    return 0;
}