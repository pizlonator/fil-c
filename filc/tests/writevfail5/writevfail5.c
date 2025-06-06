#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    struct iovec iov[1];
    char* buf = malloc(100);
    strcpy(buf, "test data");
    
    iov[0].iov_base = buf;
    iov[0].iov_len = strlen(buf);

    // Free the buffer but iovec still points to it
    free(opaque(buf));
    writev(fds[1], iov, 1);
    return 0;
}