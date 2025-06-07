#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    struct stat st;
    char* p = malloc(64);
    strcpy(p, "filc/tests/lstatfail12/lstatfail12.c");
    lstat(p - 1000, &st);
    return 0;
}