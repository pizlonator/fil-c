#include <sys/socket.h>
#include <stdfil.h>
#include <stdlib.h>
#include <inttypes.h>
#include "utils.h"

int main()
{
    int socks[2];
    ZASSERT(!socketpair(AF_UNIX, SOCK_STREAM, 0, socks));

    struct msghdr* msg = opaque(malloc(sizeof(struct msghdr)));
    msg -= (uintptr_t)msg;
    msg += UINTPTR_MAX - 10;
    
    sendmsg(socks[0], msg, 0);
    return 0;
}