#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <string.h>

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    char buf[100];
    strcpy(buf, "test data for readv");
    ZASSERT(write(fds[1], buf, strlen(buf)) == strlen(buf));

    struct iovec iov[1];
    char readbuf[50];
    
    iov[0].iov_base = readbuf;
    iov[0].iov_len = sizeof(readbuf);

    // Out-of-bounds iovec access - massive iovcnt
    readv(fds[0], iov, 50000);
    return 0;
}