#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <fcntl.h>

int main()
{
    // Cast integer to struct stat* (invalid pointer)
    fstatat(AT_FDCWD, "filc/tests/fstatatfail2/fstatatfail2.c", (struct stat*)0x123456789abcdef0ULL, 0);
    return 0;
}