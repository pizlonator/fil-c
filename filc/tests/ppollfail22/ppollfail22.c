#define _GNU_SOURCE
#include <poll.h>
#include <stdint.h>
#include <stddef.h>
#include <signal.h>
#include <stdfil.h>

int main()
{
    sigset_t* sigmask = (sigset_t*)(uintptr_t)12345678;
    ppoll(NULL, 0, NULL, sigmask);
    return 0;
}