#include <sys/stat.h>
#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int fd = open("filc/tests/fstatfail8/fstatfail8.c", O_RDONLY);
    ZASSERT(fd >= 0);
    
    struct stat* st = (struct stat*)zweak_map_new();
    fstat(fd, st);
    return 0;
}