#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    // Pass a string literal (read-only memory) as readfds
    select(FD_SETSIZE, (fd_set*)"hello this is a read-only string that is at least 128 bytes long to ensure we have enough space for an fd_set structure which is 128 bytes in size", NULL, NULL, NULL);
    
    return 0;
}
