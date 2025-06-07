#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    struct stat st;
    lstat(NULL, &st);
    return 0;
}