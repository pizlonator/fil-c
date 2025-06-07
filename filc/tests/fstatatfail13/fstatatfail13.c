#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

int main()
{
    struct stat st;
    char* p = malloc(64);
    strcpy(p, "filc/tests/fstatatfail13/fstatatfail13.c");
    fstatat(AT_FDCWD, p + 1000, &st, 0);
    return 0;
}