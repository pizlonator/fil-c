#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>

int main()
{
    // Use an integer cast to pointer as readfds
    select(1, (fd_set*)0x1010101010101010UL, NULL, NULL, NULL);
    
    return 0;
}
