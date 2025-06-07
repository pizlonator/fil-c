#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    struct stat st;
    char* p = malloc(64);
    strcpy(p, "filc/tests/statfail11/statfail11.c");
    free(p);
    stat(p, &st);
    return 0;
}