#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <string.h>

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    char buf[100];
    strcpy(buf, "test data for readv - this is a longer string");
    ZASSERT(write(fds[1], buf, strlen(buf)) == strlen(buf));

    struct iovec iov[1];
    char readbuf[20];
    
    iov[0].iov_base = readbuf;
    iov[0].iov_len = 1000;  // Buffer is only 20 bytes but asking for 1000

    readv(fds[0], iov, 1);
    return 0;
}