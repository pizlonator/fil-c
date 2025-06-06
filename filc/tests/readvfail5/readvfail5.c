#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    char buf[100];
    strcpy(buf, "test data for readv");
    ZASSERT(write(fds[1], buf, strlen(buf)) == strlen(buf));

    struct iovec iov[1];
    char* readbuf = malloc(50);
    
    iov[0].iov_base = readbuf;
    iov[0].iov_len = 50;

    // Free the buffer but still try to read into it
    free(opaque(readbuf));
    readv(fds[0], iov, 1);
    return 0;
}