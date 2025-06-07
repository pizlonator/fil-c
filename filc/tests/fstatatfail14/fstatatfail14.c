#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>

int main()
{
    struct stat st;
    char* p = malloc(64);
    strcpy(p, "filc/tests/fstatatfail14/fstatatfail14.c");
    // Cause overflow by adding a large value that will wrap around
    fstatat(AT_FDCWD, (char*)((uintptr_t)p + UINTPTR_MAX), &st, 0);
    return 0;
}