#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    struct stat st;
    char* p = malloc(64);
    strcpy(p, "filc/tests/lstatfail11/lstatfail11.c");
    free(p);
    lstat(p, &st);
    return 0;
}