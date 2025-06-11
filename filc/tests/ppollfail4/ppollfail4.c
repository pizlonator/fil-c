#define _GNU_SOURCE
#include <poll.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Allocate space for 1 struct pollfd, but tell ppoll we have 100
    struct pollfd* fds = (struct pollfd*)malloc(sizeof(struct pollfd));
    fds = (struct pollfd*)opaque(fds);
    ppoll(fds, 100, NULL, NULL);
    return 0;
}