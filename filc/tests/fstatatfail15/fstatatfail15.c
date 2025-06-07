#include <sys/stat.h>
#include <stdfil.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

int main()
{
    struct stat st;
    char* p = malloc(16);
    memset(p, 'x', 16);  // Fill entire buffer with 'x', no null terminator
    
    fstatat(AT_FDCWD, p, &st, 0);
    return 0;
}