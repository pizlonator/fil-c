#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <string.h>

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    struct iovec iov[1];
    
    // Corrupted iovec with NULL iov_base but non-zero iov_len
    iov[0].iov_base = NULL;
    iov[0].iov_len = 100;  // This is evil - NULL pointer with positive length

    writev(fds[1], iov, 1);
    return 0;
}