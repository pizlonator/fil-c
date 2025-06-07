#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    struct stat st;
    // Cast integer to char* (invalid pointer)
    lstat((char*)0x123456789abcdef0ULL, &st);
    return 0;
}