#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>
#include "utils.h"

int main()
{
    // Create pointer with valid capability but address that will overflow
    fd_set* exceptfds = opaque(malloc(sizeof(fd_set)));
    exceptfds = (fd_set*)((char*)exceptfds - (uintptr_t)exceptfds);  // Zero out address, keep capability
    exceptfds = (fd_set*)((char*)exceptfds + UINTPTR_MAX - 10);   // Near max address
    
    // This should overflow when syscall validates the fd_set buffer
    select(FD_SETSIZE, NULL, NULL, exceptfds, NULL);
    
    return 0;
}
