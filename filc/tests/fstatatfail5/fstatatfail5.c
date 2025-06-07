#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <fcntl.h>

int main()
{
    struct stat* st = malloc(sizeof(struct stat));
    fstatat(AT_FDCWD, "filc/tests/fstatatfail5/fstatatfail5.c", st + 1000, 0);
    return 0;
}