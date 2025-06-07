#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    struct stat* st = malloc(sizeof(struct stat));
    free(st);
    lstat("filc/tests/lstatfail3/lstatfail3.c", st);
    return 0;
}