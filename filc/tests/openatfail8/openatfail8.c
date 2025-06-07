#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    // O_CREAT requires mode argument but we're not providing it
    openat(AT_FDCWD, "filc/test-output/openatfail8/test.txt", O_CREAT | O_RDWR);
    return 0;
}