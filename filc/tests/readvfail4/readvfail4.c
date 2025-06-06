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
    char readbuf[20];
    
    // Try to read into read-only memory - string literal
    iov[0].iov_base = (void*)"hello";
    iov[0].iov_len = 5;

    readv(fds[0], iov, 1);
    return 0;
}