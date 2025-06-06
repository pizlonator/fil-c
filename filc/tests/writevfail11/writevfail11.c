#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <string.h>

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    struct iovec iov[2];
    char buf1[50];
    strcpy(buf1, "overlapping buffer test");
    
    // Evil: overlapping buffers in same iovec array
    iov[0].iov_base = buf1;
    iov[0].iov_len = 20;
    iov[1].iov_base = buf1 + 10;  // Overlaps with first buffer
    iov[1].iov_len = 15;

    // This is actually valid behavior in vectored I/O, just weird
    writev(fds[1], iov, 2);
    return 0;
}