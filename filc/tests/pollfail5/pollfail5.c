#include <poll.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Use a string literal which is readonly
    struct pollfd* fds = (struct pollfd*)opaque("Hello, world! This is a readonly string literal");
    poll(fds, 1, 0);
    return 0;
}