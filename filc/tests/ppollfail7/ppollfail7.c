#define _GNU_SOURCE
#include <poll.h>
#include <stddef.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    struct pollfd* fds = (struct pollfd*)opaque(zweak_map_new());
    ppoll(fds, 1, NULL, NULL);
    return 0;
}