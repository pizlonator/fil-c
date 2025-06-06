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
    strcpy(buf, "valid test case for writev");
    
    iov[0].iov_base = buf;
    iov[0].iov_len = strlen(buf);

    // This should succeed - control test
    ZASSERT(writev(fds[1], iov, 1) == strlen(buf));
    
    char readbuf[100];
    ZASSERT(read(fds[0], readbuf, sizeof(readbuf)) == strlen(buf));
    readbuf[strlen(buf)] = '\0';
    ZASSERT(strcmp(buf, readbuf) == 0);
    
    return 0;
}