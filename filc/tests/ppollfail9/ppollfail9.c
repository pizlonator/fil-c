#define _GNU_SOURCE
#include <poll.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    struct pollfd* fds = (struct pollfd*)malloc(sizeof(struct pollfd));
    fds = (struct pollfd*)opaque(fds);
    // Create a pointer that will overflow when nfds is considered
    fds = (struct pollfd*)((uintptr_t)fds + SIZE_MAX - 1000);
    ppoll(fds, 1000, NULL, NULL);
    return 0;
}