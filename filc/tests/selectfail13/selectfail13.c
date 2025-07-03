#include <sys/select.h>
#include <stdfil.h>
#include <stdlib.h>
#include "utils.h"

int main()
{
    fd_set* writefds = malloc(sizeof(fd_set));
    FD_ZERO(writefds);
    
    // Free the memory and then use it
    free(opaque(writefds));
    select(1, NULL, writefds, NULL, NULL);
    
    return 0;
}
