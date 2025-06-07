#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <fcntl.h>

int main()
{
    struct stat st;
    // Cast integer to char* (invalid pointer)
    fstatat(AT_FDCWD, (char*)0x123456789abcdef0ULL, &st, 0);
    return 0;
}