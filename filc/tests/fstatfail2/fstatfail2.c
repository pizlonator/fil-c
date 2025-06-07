#include <sys/stat.h>
#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>

int main()
{
    int fd = open("filc/tests/fstatfail2/fstatfail2.c", O_RDONLY);
    ZASSERT(fd >= 0);
    
    fstat(fd, (struct stat*)0x1010101010101010UL);
    return 0;
}