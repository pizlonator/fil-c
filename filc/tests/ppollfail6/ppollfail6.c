#define _GNU_SOURCE
#include <poll.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    struct pollfd* fds = (struct pollfd*)malloc(sizeof(struct pollfd));
    fds = (struct pollfd*)opaque(fds);
    free(fds);
    ppoll(fds, 1, NULL, NULL);
    return 0;
}