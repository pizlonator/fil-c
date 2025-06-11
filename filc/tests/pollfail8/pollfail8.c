#include <poll.h>
#include <stdint.h>
#include <stdfil.h>

int main()
{
    struct pollfd* fds = (struct pollfd*)(uintptr_t)12345678;
    poll(fds, 1, 0);
    return 0;
}