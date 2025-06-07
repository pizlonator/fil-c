#include <sys/stat.h>
#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int fd = open("filc/tests/fstatfail5/fstatfail5.c", O_RDONLY);
    ZASSERT(fd >= 0);
    
    struct stat* st = malloc(sizeof(struct stat));
    fstat(fd, st + 1000);
    return 0;
}