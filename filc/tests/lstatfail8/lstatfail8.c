#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    struct stat* st = (struct stat*)zweak_map_new();
    lstat("filc/tests/lstatfail8/lstatfail8.c", st);
    return 0;
}