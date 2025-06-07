#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    struct stat* st = malloc(sizeof(struct stat));
    lstat("filc/tests/lstatfail4/lstatfail4.c", st - 1000);
    return 0;
}