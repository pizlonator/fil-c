#define _GNU_SOURCE
#include <poll.h>
#include <stdlib.h>
#include <stddef.h>
#include <signal.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    sigset_t* sigmask = (sigset_t*)malloc(sizeof(sigset_t));
    sigmask = (sigset_t*)opaque(sigmask);
    ppoll(NULL, 0, NULL, sigmask + 1000);
    return 0;
}