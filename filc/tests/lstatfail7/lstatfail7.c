#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    // malloc(0) allocates but with 0 space
    struct stat* st = (struct stat*)malloc(0);
    lstat("filc/tests/lstatfail7/lstatfail7.c", st);
    return 0;
}