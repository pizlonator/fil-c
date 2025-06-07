#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <fcntl.h>

int main()
{
    // malloc(0) allocates but with 0 space
    struct stat* st = (struct stat*)malloc(0);
    fstatat(AT_FDCWD, "filc/tests/fstatatfail7/fstatatfail7.c", st, 0);
    return 0;
}