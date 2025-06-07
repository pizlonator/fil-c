#include <fcntl.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    // O_CREAT requires mode argument but we're not providing it
    openat(AT_FDCWD, "filc/test-output/openatfail7/test.txt", O_CREAT | O_WRONLY);
    return 0;
}