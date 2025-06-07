#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>

int main()
{
    openat(AT_FDCWD, (char*)0x1010101010101010UL, O_RDONLY);
    return 0;
}