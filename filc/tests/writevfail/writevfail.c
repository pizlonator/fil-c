#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    // NULL iovec pointer
    writev(fds[1], NULL, 1);
    return 0;
}