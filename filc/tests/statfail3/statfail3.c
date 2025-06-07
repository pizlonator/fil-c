#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    struct stat* st = malloc(sizeof(struct stat));
    free(st);
    stat("filc/tests/statfail3/statfail3.c", st);
    return 0;
}