#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>
#include "utils.h"

int main()
{
    // Create pointer with valid capability but address that will overflow
    fd_set* writefds = opaque(malloc(sizeof(fd_set)));
    writefds = (fd_set*)((char*)writefds - (uintptr_t)writefds);  // Zero out address, keep capability
    writefds = (fd_set*)((char*)writefds + UINTPTR_MAX - 10);   // Near max address
    
    // This should overflow when syscall validates the fd_set buffer
    select(1, NULL, writefds, NULL, NULL);
    
    return 0;
}
