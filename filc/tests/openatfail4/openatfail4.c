#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    char* p = malloc(16);
    strcpy(p, "test.txt");
    openat(AT_FDCWD, p - 1000, O_RDONLY);
    return 0;
}