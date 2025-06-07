#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <fcntl.h>

int main()
{
    struct stat* st = malloc(sizeof(struct stat));
    free(st);
    fstatat(AT_FDCWD, "filc/tests/fstatatfail3/fstatatfail3.c", st, 0);
    return 0;
}