#define _GNU_SOURCE
#include <poll.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <signal.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    sigset_t* sigmask = (sigset_t*)malloc(sizeof(sigset_t));
    sigmask = (sigset_t*)opaque(sigmask);
    // Create a pointer that will overflow
    sigmask = (sigset_t*)((uintptr_t)sigmask + SIZE_MAX - 1000);
    ppoll(NULL, 0, NULL, sigmask);
    return 0;
}