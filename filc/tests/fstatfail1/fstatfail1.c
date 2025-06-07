#include <sys/stat.h>
#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    int fd = open("filc/tests/fstatfail1/fstatfail1.c", O_RDONLY);
    ZASSERT(fd >= 0);
    
    fstat(fd, NULL);
    return 0;
}