#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int main()
{
    struct stat st;
    char* p = malloc(64);
    strcpy(p, "filc/tests/statfail14/statfail14.c");
    // Cause overflow by adding a large value that will wrap around
    stat((char*)((uintptr_t)p + UINTPTR_MAX), &st);
    return 0;
}