#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>

int main()
{
    // Pass a string literal (read-only memory) as writefds
    select(1, NULL, (fd_set*)"hello this is a read-only string that is at least 128 bytes long to ensure we have enough space for an fd_set structure which is 128 bytes in size", NULL, NULL);
    
    return 0;
}
