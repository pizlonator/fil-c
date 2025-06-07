#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <string.h>
#include <inttypes.h>

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    // Evil: iovec array pointer is a raw integer cast
    struct iovec* evil_iov = (struct iovec*)0xDEADBEEFCAFEBABEUL;

    writev(fds[1], evil_iov, 2);
    return 0;
}