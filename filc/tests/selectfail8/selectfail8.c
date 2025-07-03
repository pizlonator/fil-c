#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>
#include "utils.h"

int main()
{
    // Create pointer with valid capability but address that will overflow
    fd_set* readfds = opaque(malloc(sizeof(fd_set)));
    readfds = (fd_set*)((char*)readfds - (uintptr_t)readfds);  // Zero out address, keep capability
    readfds = (fd_set*)((char*)readfds + UINTPTR_MAX - 10);   // Near max address
    
    // This should overflow when syscall validates the fd_set buffer
    select(1, readfds, NULL, NULL, NULL);
    
    return 0;
}
