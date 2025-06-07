#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    // Cast integer to struct stat* (invalid pointer)
    lstat("filc/tests/lstatfail2/lstatfail2.c", (struct stat*)0x123456789abcdef0ULL);
    return 0;
}