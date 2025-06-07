#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    struct stat st;
    stat(NULL, &st);
    return 0;
}