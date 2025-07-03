#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main()
{
    fd_set* readfds = malloc(sizeof(fd_set));
    FD_ZERO(readfds);
    
    // Free the memory and then use it
    free(opaque(readfds));
    select(1, readfds, NULL, NULL, NULL);
    
    return 0;
}
