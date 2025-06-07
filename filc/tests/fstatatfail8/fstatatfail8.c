#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <fcntl.h>

int main()
{
    struct stat* st = (struct stat*)zweak_map_new();
    fstatat(AT_FDCWD, "filc/tests/fstatatfail8/fstatatfail8.c", st, 0);
    return 0;
}