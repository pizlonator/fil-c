#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <fcntl.h>

int main()
{
    struct stat st;
    fstatat(AT_FDCWD, (char*)zweak_map_new(), &st, 0);
    return 0;
}