#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <string.h>

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    struct iovec iov[1];
    
    // Super evil: iovec points to itself recursively
    iov[0].iov_base = (void*)iov;
    iov[0].iov_len = sizeof(struct iovec);

    writev(fds[1], iov, 1);
    return 0;
}