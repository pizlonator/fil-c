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
    
    // iov_len larger than actual buffer size - classic buffer overread
    iov[0].iov_base = buf;
    iov[0].iov_len = 1000;  // buf is only 50 bytes

    writev(fds[1], iov, 1);
    return 0;
}