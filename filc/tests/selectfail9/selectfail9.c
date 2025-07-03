#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    fd_set writefds;
    FD_ZERO(&writefds);
    
    // Pass a pointer that is 1000 bytes above the bounds of writefds
    select(FD_SETSIZE, NULL, (fd_set*)((char*)&writefds + 1000), NULL, NULL);
    
    return 0;
}
