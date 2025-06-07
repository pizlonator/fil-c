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

    // Evil: iovec array pointer that overflows when accessing multiple elements
    struct iovec* base_iov = opaque(malloc(sizeof(struct iovec) * 8));
    
    // Manipulate to create overflow condition
    struct iovec* evil_iov = base_iov;
    evil_iov -= (uintptr_t)evil_iov / sizeof(struct iovec);  // Zero the address
    evil_iov += (UINTPTR_MAX / sizeof(struct iovec)) - 2;    // Very high index
    
    // When writev tries to access evil_iov[0], evil_iov[1], evil_iov[2], 
    // the address calculation should overflow
    writev(fds[1], evil_iov, 5);
    return 0;
}