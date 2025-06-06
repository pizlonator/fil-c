#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <string.h>

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    struct iovec iov[3];
    char buf1[50], buf2[50];
    strcpy(buf1, "first");
    strcpy(buf2, "second");
    
    // First two are valid
    iov[0].iov_base = buf1;
    iov[0].iov_len = strlen(buf1);
    iov[1].iov_base = buf2;
    iov[1].iov_len = strlen(buf2);
    
    // Third one is evil - out of bounds low
    iov[2].iov_base = buf1 - 1000;
    iov[2].iov_len = 5;

    writev(fds[1], iov, 3);
    return 0;
}