#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    fd_set writefds;
    FD_ZERO(&writefds);
    
    // Pass a pointer that starts in bounds but will go out of bounds
    // fd_set is 128 bytes, so offset by 64 to have only 64 bytes remaining
    select(FD_SETSIZE, NULL, (fd_set*)((char*)&writefds + 64), NULL, NULL);
    
    return 0;
}
