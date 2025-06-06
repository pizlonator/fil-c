#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <string.h>

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    struct iovec iov[1];
    char buf[50];
    strcpy(buf, "test data");
    
    // Out-of-bounds buffer pointer
    iov[0].iov_base = buf + 1000;
    iov[0].iov_len = 10;

    writev(fds[1], iov, 1);
    return 0;
}