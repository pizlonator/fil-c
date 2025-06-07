#include <sys/stat.h>
#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    struct stat st;
    fstatat(AT_FDCWD, NULL, &st, 0);
    return 0;
}