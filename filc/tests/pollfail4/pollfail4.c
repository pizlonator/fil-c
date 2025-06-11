#include <poll.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Allocate space for 1 struct pollfd, but tell poll we have 100
    struct pollfd* fds = (struct pollfd*)malloc(sizeof(struct pollfd));
    fds = (struct pollfd*)opaque(fds);
    poll(fds, 100, 0);
    return 0;
}