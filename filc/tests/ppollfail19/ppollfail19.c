#define _GNU_SOURCE
#include <poll.h>
#include <stdlib.h>
#include <stddef.h>
#include <signal.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    // Allocate too little space for a sigset_t - use malloc(1) and point past the end
    char* small_buf = (char*)malloc(1);
    small_buf = (char*)opaque(small_buf);
    sigset_t* sigmask = (sigset_t*)(small_buf + 8);
    ppoll(NULL, 0, NULL, sigmask);
    return 0;
}