#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    
    // Pass a pointer that is 1000 bytes below the bounds of readfds
    select(1, (fd_set*)((char*)&readfds - 1000), NULL, NULL, NULL);
    
    return 0;
}
