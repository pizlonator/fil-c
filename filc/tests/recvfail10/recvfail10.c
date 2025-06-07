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

    char buf[100];
    unsigned index;
    for (index = 100; index--;)
        buf[index] = (char)(index + 42);

    ZASSERT(send(socks[0], buf, 100, 0) == 100);

    char* p = opaque(malloc(16));
    p -= (uintptr_t)p;
    p += UINTPTR_MAX - 10;
    
    recv(socks[1], p, 50, 0);
    return 0;
}