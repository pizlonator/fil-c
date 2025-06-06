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
    strcpy(buf1, "first");
    
    iov[0].iov_base = buf1;
    iov[0].iov_len = strlen(buf1);
    
    // Second iovec points to special object - very evil
    iov[1].iov_base = zweak_map_new();
    iov[1].iov_len = 8;

    writev(fds[1], iov, 2);
    return 0;
}