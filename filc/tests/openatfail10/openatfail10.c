#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    void* p = zweak_map_new();
    openat(AT_FDCWD, p, O_RDONLY);
    return 0;
}