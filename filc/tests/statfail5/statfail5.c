#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    struct stat* st = malloc(sizeof(struct stat));
    stat("filc/tests/statfail5/statfail5.c", st + 1000);
    return 0;
}