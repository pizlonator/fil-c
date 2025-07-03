#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    fd_set exceptfds;
    FD_ZERO(&exceptfds);
    
    // Pass a pointer that starts in bounds but will go out of bounds
    // fd_set is 128 bytes, so offset by 64 to have only 64 bytes remaining
    select(FD_SETSIZE, NULL, NULL, (fd_set*)((char*)&exceptfds + 64), NULL);
    
    return 0;
}
