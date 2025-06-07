#include <sys/socket.h>
#include <stdfil.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "utils.h"

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    char* p = opaque(malloc(16));
    p -= (uintptr_t)p;
    p += UINTPTR_MAX - 5;
    
    send(socks[0], p, 50, 0);
    return 0;
}