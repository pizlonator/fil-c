#define _GNU_SOURCE
#include <poll.h>
#include <stdint.h>
#include <stddef.h>
#include <stdfil.h>

int main()
{
    struct pollfd* fds = (struct pollfd*)(uintptr_t)12345678;
    ppoll(fds, 1, NULL, NULL);
    return 0;
}