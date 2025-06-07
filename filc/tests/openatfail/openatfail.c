#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    openat(AT_FDCWD, NULL, O_RDONLY);
    return 0;
}