#include <poll.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    struct pollfd* fds = (struct pollfd*)malloc(sizeof(struct pollfd));
    fds = (struct pollfd*)opaque(fds);
    free(fds);
    poll(fds, 1, 0);
    return 0;
}