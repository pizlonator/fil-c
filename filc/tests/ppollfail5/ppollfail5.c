#define _GNU_SOURCE
#include <poll.h>
#include <stddef.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Use a string literal which is readonly
    struct pollfd* fds = (struct pollfd*)opaque("Hello, world! This is a readonly string literal");
    ppoll(fds, 1, NULL, NULL);
    return 0;
}