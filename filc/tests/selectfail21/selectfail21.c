#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main()
{
    fd_set* exceptfds = malloc(sizeof(fd_set));
    FD_ZERO(exceptfds);
    
    // Free the memory and then use it
    free(opaque(exceptfds));
    select(1, NULL, NULL, exceptfds, NULL);
    
    return 0;
}
