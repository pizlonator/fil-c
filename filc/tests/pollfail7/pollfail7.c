#include <poll.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    struct pollfd* fds = (struct pollfd*)opaque(zweak_map_new());
    poll(fds, 1, 0);
    return 0;
}