#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

int main()
{
    struct stat st;
    char* p = malloc(64);
    strcpy(p, "filc/tests/fstatatfail11/fstatatfail11.c");
    free(p);
    fstatat(AT_FDCWD, p, &st, 0);
    return 0;
}