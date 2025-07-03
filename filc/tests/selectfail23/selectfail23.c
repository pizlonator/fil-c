#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>

int main()
{
    // Use an integer cast to pointer as exceptfds
    select(1, NULL, NULL, (fd_set*)0x1010101010101010UL, NULL);
    
    return 0;
}
