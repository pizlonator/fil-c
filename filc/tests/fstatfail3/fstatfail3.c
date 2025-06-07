#include <sys/stat.h>
#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int fd = open("filc/tests/fstatfail3/fstatfail3.c", O_RDONLY);
    ZASSERT(fd >= 0);
    
    struct stat* st = malloc(sizeof(struct stat));
    free(st);
    fstat(fd, st);
    return 0;
}