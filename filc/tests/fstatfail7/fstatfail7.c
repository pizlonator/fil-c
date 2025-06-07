#include <sys/stat.h>
#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int fd = open("filc/tests/fstatfail7/fstatfail7.c", O_RDONLY);
    ZASSERT(fd >= 0);
    
    struct stat* st = malloc(0);  // Zero-sized allocation
    fstat(fd, st);
    return 0;
}