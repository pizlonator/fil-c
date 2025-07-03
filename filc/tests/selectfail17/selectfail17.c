#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    fd_set exceptfds;
    FD_ZERO(&exceptfds);
    
    // Pass a pointer that is 1000 bytes above the bounds of exceptfds
    select(FD_SETSIZE, NULL, NULL, (fd_set*)((char*)&exceptfds + 1000), NULL);
    
    return 0;
}
