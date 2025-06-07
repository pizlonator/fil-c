#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <fcntl.h>

int main()
{
    struct stat* st = malloc(sizeof(struct stat));
    fstatat(AT_FDCWD, "filc/tests/fstatatfail4/fstatatfail4.c", st - 1000, 0);
    return 0;
}