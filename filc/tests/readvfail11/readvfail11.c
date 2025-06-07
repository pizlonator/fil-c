#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <string.h>
#include <inttypes.h>

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    char buf[100];
    strcpy(buf, "test data for readv");
    ZASSERT(write(fds[1], buf, strlen(buf)) == strlen(buf));

    // Evil: iovec array pointer is a raw integer cast
    struct iovec* evil_iov = (struct iovec*)0x1010101010101010UL;

    readv(fds[0], evil_iov, 1);
    return 0;
}