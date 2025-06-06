#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <string.h>

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    struct iovec iov[1];
    char buf[100];
    strcpy(buf, "test data");
    
    iov[0].iov_base = buf;
    iov[0].iov_len = strlen(buf);

    // Out-of-bounds iovec access - asking for 1000 iovecs when we only have 1
    writev(fds[1], iov, 1000);
    return 0;
}