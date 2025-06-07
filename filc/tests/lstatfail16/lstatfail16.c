#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    struct stat st;
    lstat((char*)zweak_map_new(), &st);
    return 0;
}