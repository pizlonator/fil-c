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

    struct iovec iov[2];
    char readbuf1[20], readbuf2[20];
    
    // First iovec is valid
    iov[0].iov_base = readbuf1;
    iov[0].iov_len = sizeof(readbuf1);
    
    // Second iovec tries to read into out-of-bounds memory
    iov[1].iov_base = readbuf2 + 1000;
    iov[1].iov_len = 10;

    readv(fds[0], iov, 2);
    return 0;
}