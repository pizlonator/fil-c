#include <unistd.h>
#include <stdfil.h>
#include <sys/uio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "utils.h"

int main()
{
    int fds[2];
    ZASSERT(!pipe(fds));

    char buf[100];
    strcpy(buf, "test data for readv overflow");
    ZASSERT(write(fds[1], buf, strlen(buf)) == strlen(buf));

    // Evil: iovec array pointer that overflows when iovcnt is added
    struct iovec* base_iov = opaque(malloc(sizeof(struct iovec) * 4));
    
    // Manipulate pointer to be near overflow boundary
    struct iovec* evil_iov = base_iov;
    evil_iov -= (uintptr_t)evil_iov / sizeof(struct iovec);  // Zero address, keep capability
    evil_iov += (UINTPTR_MAX / sizeof(struct iovec)) - 1;    // Near max elements
    
    // When readv computes evil_iov + iovcnt * sizeof(struct iovec), it should overflow
    readv(fds[0], evil_iov, 10);
    return 0;
}